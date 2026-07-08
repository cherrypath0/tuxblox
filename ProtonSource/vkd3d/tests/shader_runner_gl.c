/*
 * Copyright 2023 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#ifdef HAVE_OPENGL

#ifndef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#define COBJMACROS
#define VKD3D_TEST_NO_DEFS
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include "shader_runner.h"
#include "vkd3d_d3dcompiler.h"

static PFNGLDEPTHBOUNDSEXTPROC p_glDepthBoundsEXT;
static PFNGLSPECIALIZESHADERPROC p_glSpecializeShader;
static PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC p_glDrawArraysInstancedBaseInstance;

enum shading_language
{
    GLSL,
    SPIR_V,
};

struct format_info
{
    enum DXGI_FORMAT f;
    unsigned int component_count;
    bool is_integer;
    bool is_shadow;
    GLenum internal_format;
    GLenum format;
    GLenum type;
};

struct gl_resource
{
    struct resource r;

    const struct format_info *format;
    GLuint id, tbo_id;
    GLenum target;
};

static struct gl_resource *gl_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct gl_resource, r);
}

struct gl_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;

    EGLDisplay display;
    EGLContext context;

    uint32_t attribute_map;
    GLuint fbo_id;

    enum vkd3d_shader_tessellator_output_primitive output_primitive;
    enum vkd3d_shader_tessellator_partitioning partitioning;

    struct vkd3d_shader_combined_resource_sampler *combined_samplers;
    unsigned int combined_sampler_count;
    enum shading_language language;

    uint32_t clip_distance_mask[SHADER_TYPE_COUNT];
};

static struct gl_runner *gl_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct gl_runner, r);
}

static void debug_output(GLenum source, GLenum type, GLuint id, GLenum severity,
        GLsizei length, const GLchar *message, const void *userParam)
{
    if (message[length - 1] == '\n')
        --length;
    trace("%.*s\n", length, message);
}

static bool check_extension(GLenum name, const char *extension, GLint extension_count)
{
    for (GLint i = 0; i < extension_count; ++i)
    {
        if (!strcmp(extension, (const char *)glGetStringi(name, i)))
            return true;
    }

    return false;
}

static bool check_gl_extension(const char *extension, GLint extension_count)
{
    return check_extension(GL_EXTENSIONS, extension, extension_count);
}

static bool check_spirv_extension(const char *extension, GLint extension_count)
{
    return check_extension(GL_SPIR_V_EXTENSIONS, extension, extension_count);
}

static bool check_gl_extensions(struct gl_runner *runner)
{
    GLint count, spirv_count = 0;

    static const char *required_extensions[] =
    {
        "GL_ARB_base_instance",
        "GL_ARB_clip_control",
        "GL_ARB_compute_shader",
        "GL_ARB_copy_image",
        "GL_ARB_internalformat_query",
        "GL_ARB_sampler_objects",
        "GL_ARB_shader_image_load_store",
        "GL_ARB_texture_storage",
    };

    glGetIntegerv(GL_NUM_EXTENSIONS, &count);

    if (runner->language == SPIR_V)
    {
        if (!check_gl_extension("GL_ARB_gl_spirv", count))
            return false;

        if (check_gl_extension("GL_ARB_spirv_extensions", count))
            glGetIntegerv(GL_NUM_SPIR_V_EXTENSIONS, &spirv_count);
    }

    for (unsigned int i = 0; i < ARRAY_SIZE(required_extensions); ++i)
    {
        if (!check_gl_extension(required_extensions[i], count))
            return false;
    }

    if (check_gl_extension("GL_EXT_depth_bounds_test", count))
        runner->caps.shader_caps[SHADER_CAP_DEPTH_BOUNDS] = true;
    if (check_gl_extension("GL_ARB_cull_distance", count))
        runner->caps.shader_caps[SHADER_CAP_CULL_DISTANCE] = true;
    if (check_gl_extension("GL_ARB_gpu_shader_fp64", count))
        runner->caps.shader_caps[SHADER_CAP_FLOAT64] = true;
    if (check_gl_extension("GL_ARB_gpu_shader_int64", count))
        runner->caps.shader_caps[SHADER_CAP_INT64] = true;
    if (check_gl_extension("GL_ARB_shader_viewport_layer_array", count) && (runner->language == GLSL
            || check_spirv_extension("SPV_EXT_shader_viewport_index_layer", spirv_count)))
        runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX] = true;
    if (check_gl_extension("GL_ARB_tessellation_shader", count))
        runner->caps.shader_caps[SHADER_CAP_TESSELLATION_SHADER] = true;
    if (check_gl_extension("GL_ARB_texture_filter_minmax", count))
        runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX] = true;

    return true;
}

static bool check_egl_client_extension(const char *extension)
{
    const char *extensions, *p;
    size_t len;

    if (!(extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS)))
        return false;

    len = strlen(extension);
    for (;;)
    {
        if (!(p = strchr(extensions, ' ')))
            p = &extensions[strlen(extensions)];
        if (p - extensions == len && !memcmp(extensions, extension, len))
            return true;
        if (!*p)
            break;
        extensions = p + 1;
    }
    return false;
}

static bool check_glsl_support(void)
{
    const enum vkd3d_shader_target_type *target_types;
    unsigned int count, i;

    target_types = vkd3d_shader_get_supported_target_types(VKD3D_SHADER_SOURCE_DXBC_TPF, &count);
    for (i = 0; i < count; ++i)
    {
        if (target_types[i] == VKD3D_SHADER_TARGET_GLSL)
            return true;
    }

    return false;
}

static const struct format_info *get_format_info(enum DXGI_FORMAT format, bool is_shadow)
{
    size_t i;

    static const struct format_info format_info[] =
    {
        {DXGI_FORMAT_UNKNOWN,            1, true,  false, GL_R32UI,              GL_RED_INTEGER,     GL_UNSIGNED_INT},
        {DXGI_FORMAT_R32G32B32A32_FLOAT, 4, false, false, GL_RGBA32F,            GL_RGBA,            GL_FLOAT},
        {DXGI_FORMAT_R32G32B32A32_UINT,  4, true,  false, GL_RGBA32UI,           GL_RGBA_INTEGER,    GL_UNSIGNED_INT},
        {DXGI_FORMAT_R32G32B32A32_SINT,  4, true,  false, GL_RGBA32I,            GL_RGBA_INTEGER,    GL_INT},
        {DXGI_FORMAT_R32G32_FLOAT,       2, false, false, GL_RG32F,              GL_RG,              GL_FLOAT},
        {DXGI_FORMAT_R32G32_UINT,        2, true,  false, GL_RG32UI,             GL_RG_INTEGER,      GL_UNSIGNED_INT},
        {DXGI_FORMAT_R32G32_SINT,        2, true,  false, GL_RG32I,              GL_RG_INTEGER,      GL_INT},
        {DXGI_FORMAT_R32_FLOAT,          1, false, false, GL_R32F,               GL_RED,             GL_FLOAT},
        {DXGI_FORMAT_R32_FLOAT,          1, false, true,  GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
        {DXGI_FORMAT_D32_FLOAT,          1, false, true,  GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
        {DXGI_FORMAT_R32_UINT,           1, true,  false, GL_R32UI,              GL_RED_INTEGER,     GL_UNSIGNED_INT},
        {DXGI_FORMAT_R32_SINT,           1, true,  false, GL_R32I,               GL_RED_INTEGER,     GL_INT},
        {DXGI_FORMAT_R32_TYPELESS,       1, true,  false, GL_R32UI,              GL_RED_INTEGER,     GL_UNSIGNED_INT},
    };

    for (i = 0; i < ARRAY_SIZE(format_info); ++i)
    {
        if (format_info[i].f == format && format_info[i].is_shadow == is_shadow)
            return &format_info[i];
    }

    fatal_error("Failed to find format info for format %#x.\n", format);
}

static uint32_t get_format_support(struct gl_runner *runner, enum DXGI_FORMAT format)
{
    GLenum gl_format = get_format_info(format, false)->internal_format;
    uint32_t ret = 0;
    GLint support;

    /* TODO: Probably check for more targets instead of just GL_TEXTURE_2D. */
    glGetInternalformativ(GL_TEXTURE_2D, gl_format, GL_SHADER_IMAGE_LOAD, 1, &support);
    if (support != GL_NONE)
        ret |= FORMAT_CAP_UAV_LOAD;

    return ret;
}

static bool get_mesa_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    return find_version_string((const char *)glGetString(GL_VERSION), "Mesa ", major, minor, patch);
}

static bool get_llvm_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    return find_version_string((const char *)glGetString(GL_RENDERER), "LLVM ", major, minor, patch);
}

static bool gl_runner_init(struct gl_runner *runner, enum shading_language language)
{
    PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT;
    const char *glsl_version = NULL;
    EGLint count, extension_count;
    EGLDeviceEXT *devices;
    EGLContext context;
    EGLDisplay display;
    EGLBoolean ret;
    GLuint vao;

    static const EGLint attributes[] =
    {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE,
    };

    static const enum DXGI_FORMAT formats[] =
    {
        DXGI_FORMAT_UNKNOWN,
        DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_R32_UINT,
        DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_UINT,
        DXGI_FORMAT_R32G32B32A32_SINT,
    };

    if (language == GLSL && !check_glsl_support())
    {
        skip("GLSL support is not enabled. If this is unintentional, "
                "add -DVKD3D_SHADER_UNSUPPORTED_GLSL to CPPFLAGS.\n");
        return false;
    }

    memset(runner, 0, sizeof(*runner));
    runner->language = language;

    if (!check_egl_client_extension("EGL_EXT_device_enumeration")
            || !(eglQueryDevicesEXT = (void *)eglGetProcAddress("eglQueryDevicesEXT")))
    {
        skip("Failed to retrieve eglQueryDevicesEXT.\n");
        return false;
    }

    ret = eglQueryDevicesEXT(0, NULL, &count);
    ok(ret, "Failed to query device count.\n");

    devices = calloc(count, sizeof(*devices));
    ret = eglQueryDevicesEXT(count, devices, &count);
    ok(ret, "Failed to query devices.\n");

    for (unsigned int i = 0; i < count; ++i)
    {
        if ((display = eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devices[i], NULL)) == EGL_NO_DISPLAY)
        {
            trace("Failed to get EGL display connection for device %u.\n", i);
            continue;
        }

        if (!eglInitialize(display, NULL, NULL))
        {
            trace("Failed to initialise EGL display connection for device %u.\n", i);
            continue;
        }

        if (!eglBindAPI(EGL_OPENGL_API))
        {
            trace("Failed to bind OpenGL API for device %u.\n", i);
            eglTerminate(display);
            continue;
        }

        if ((context = eglCreateContext(display, NULL, EGL_NO_CONTEXT, attributes)) == EGL_NO_CONTEXT)
        {
            trace("Failed to create EGL context for device %u.\n", i);
            eglTerminate(display);
            continue;
        }

        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context))
        {
            trace("Failed to make EGL context current for device %u.\n", i);
            eglDestroyContext(display, context);
            eglTerminate(display);
            continue;
        }

        glsl_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        if (language == GLSL)
        {
            unsigned int major, minor;
            sscanf(glsl_version, "%u.%u", &major, &minor);
            if (major < 4 || (major == 4 && minor < 40))
            {
                trace("Device %u does not support GLSL 4.40.\n", i);
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroyContext(display, context);
                eglTerminate(display);
                continue;
            }
        }

        memset(&runner->caps, 0, sizeof(runner->caps));
        if (!check_gl_extensions(runner))
        {
            trace("Device %u lacks required extensions.\n", i);
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
            eglTerminate(display);
            continue;
        }
        runner->caps.runner = language == SPIR_V ? "OpenGL/SPIR-V" : "OpenGL/GLSL";
        runner->caps.compiler = HLSL_COMPILER;
        runner->caps.minimum_shader_model = SHADER_MODEL_4_0;
        runner->caps.maximum_shader_model = SHADER_MODEL_5_1;
        runner->caps.shader_caps[SHADER_CAP_GEOMETRY_SHADER] = true;

        runner->caps.tags[SHADER_RUNNER_TAG_OPENGL].present = true;

        if (runner->language == GLSL)
            runner->caps.tags[SHADER_RUNNER_TAG_GLSL].present = true;

        if (strncmp((const char *)glGetString(GL_RENDERER), "llvmpipe ", 9) == 0)
            runner->caps.tags[SHADER_RUNNER_TAG_LLVMPIPE].present = true;

        runner->caps.tags[SHADER_RUNNER_TAG_MESA].present = get_mesa_version(
            &runner->caps.tags[SHADER_RUNNER_TAG_MESA].major, &runner->caps.tags[SHADER_RUNNER_TAG_MESA].minor,
            &runner->caps.tags[SHADER_RUNNER_TAG_MESA].patch);

        runner->caps.tags[SHADER_RUNNER_TAG_LLVM].present = get_llvm_version(
            &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].major, &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].minor,
            &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].patch);

        glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
        if (check_gl_extension("GL_ARB_internalformat_query2", extension_count))
        {
            for (unsigned int j = 0; j < ARRAY_SIZE(formats); ++j)
            {
                runner->caps.format_caps[formats[j]] = get_format_support(runner, formats[j]);
            }
        }

        trace("Using device %u.\n", i);
        runner->display = display;
        runner->context = context;
        break;
    }

    free(devices);

    if (!runner->context)
    {
        skip("Failed to find a usable OpenGL device.\n");
        return false;
    }

    trace("                  GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    trace("                GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    trace("                 GL_VERSION: %s\n", glGetString(GL_VERSION));
    trace("GL_SHADING_LANGUAGE_VERSION: %s\n", glsl_version);

    p_glDepthBoundsEXT = (void *)eglGetProcAddress("glDepthBoundsEXT");
    p_glSpecializeShader = (void *)eglGetProcAddress("glSpecializeShader");
    p_glDrawArraysInstancedBaseInstance = (void *)eglGetProcAddress("glDrawArraysInstancedBaseInstance");

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
    glDebugMessageCallback(debug_output, NULL);
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    glFrontFace(GL_CW);
    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    return true;
}

static void reset_combined_samplers(struct gl_runner *runner)
{
    free(runner->combined_samplers);
    runner->combined_samplers = NULL;
    runner->combined_sampler_count = 0;
}

static void gl_runner_cleanup(struct gl_runner *runner)
{
    EGLBoolean ret;

    reset_combined_samplers(runner);

    if (runner->fbo_id)
        glDeleteFramebuffers(1, &runner->fbo_id);

    ret = eglMakeCurrent(runner->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    ok(ret, "Failed to release current EGL context.\n");
    ret = eglDestroyContext(runner->display, runner->context);
    ok(ret, "Failed to destroy EGL context.\n");
    ret = eglTerminate(runner->display);
    ok(ret, "Failed to terminate EGL display connection.\n");
}

static bool init_resource_texture(struct gl_resource *resource, const struct resource_params *params)
{
    unsigned int offset, w, h, d, i;
    GLenum target;

    if (params->desc.dimension == RESOURCE_DIMENSION_2D)
    {
        if (params->desc.sample_count > 1)
            target = GL_TEXTURE_2D_MULTISAMPLE;
        else if (params->desc.layer_count > 1)
            target = GL_TEXTURE_2D_ARRAY;
        else
            target = GL_TEXTURE_2D;
    }
    else if (params->desc.dimension == RESOURCE_DIMENSION_3D)
    {
        target = GL_TEXTURE_3D;
    }
    else
    {
        target = GL_TEXTURE_CUBE_MAP;
    }
    resource->target = target;

    resource->format = get_format_info(params->desc.format, params->is_shadow);

    if (params->desc.sample_count > 1)
    {
        GLint max_sample_count;

        glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE, resource->format->internal_format, GL_SAMPLES, 1, &max_sample_count);
        if (max_sample_count < params->desc.sample_count)
        {
            trace("Format #%x with sample count %u is not supported; skipping.\n", params->desc.format, params->desc.sample_count);
            return false;
        }
    }

    glGenTextures(1, &resource->id);
    glBindTexture(target, resource->id);
    if (params->desc.sample_count > 1)
    {
        glTexStorage2DMultisample(target, params->desc.sample_count,
                resource->format->internal_format, params->desc.width, params->desc.height, GL_FALSE);
    }
    else
    {
        if (params->desc.dimension == RESOURCE_DIMENSION_3D)
            glTexStorage3D(target, params->desc.level_count, resource->format->internal_format,
                    params->desc.width, params->desc.height, params->desc.depth);
        else if (params->desc.layer_count > 1 && params->desc.dimension != RESOURCE_DIMENSION_CUBE)
            glTexStorage3D(target, params->desc.level_count, resource->format->internal_format,
                    params->desc.width, params->desc.height, params->desc.layer_count);
        else
            glTexStorage2D(target, params->desc.level_count, resource->format->internal_format,
                    params->desc.width, params->desc.height);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    }
    if (!params->data)
        return true;

    for (i = 0, offset = 0; i < params->desc.level_count; ++i)
    {
        w = get_level_dimension(params->desc.width, i);
        h = get_level_dimension(params->desc.height, i);
        d = get_level_dimension(params->desc.depth, i);

        if (params->desc.dimension == RESOURCE_DIMENSION_3D)
        {
            glTexSubImage3D(target, i, 0, 0, 0, w, h, d, resource->format->format,
                    resource->format->type, params->data + offset);
            offset += w * h * d * params->desc.texel_size;
        }
        else if (params->desc.dimension == RESOURCE_DIMENSION_CUBE)
        {
            static const GLenum faces[] =
            {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            for (unsigned int face = 0; face < 6; ++face)
            {
                glTexSubImage2D(faces[face], i, 0, 0, w, h, resource->format->format,
                        resource->format->type, params->data + offset);
                offset += w * h * params->desc.texel_size;
            }
        }
        else if (params->desc.layer_count > 1)
        {
            glTexSubImage3D(target, i, 0, 0, 0, w, h, params->desc.layer_count, resource->format->format,
                    resource->format->type, params->data + offset);
            offset += w * h * params->desc.layer_count * params->desc.texel_size;
        }
        else
        {
            glTexSubImage2D(target, i, 0, 0, w, h, resource->format->format,
                    resource->format->type, params->data + offset);
            offset += w * h * params->desc.texel_size;
        }
    }

    return true;
}

static void init_resource_buffer(struct gl_resource *resource, const struct resource_params *params)
{
    GLenum target = GL_TEXTURE_BUFFER;

    resource->format = get_format_info(params->desc.format, false);
    resource->target = target;

    glGenBuffers(1, &resource->id);
    glBindBuffer(target, resource->id);
    glBufferData(target, params->data_size, params->data, GL_STATIC_DRAW);

    glGenTextures(1, &resource->tbo_id);
    glBindTexture(target, resource->tbo_id);
    glTexBuffer(target, resource->format->internal_format, resource->id);
}

static struct resource *gl_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct gl_resource *resource;

    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UAV:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
                init_resource_buffer(resource, params);
            else if (!init_resource_texture(resource, params))
                return NULL;
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            glGenBuffers(1, &resource->id);
            glBindBuffer(GL_ARRAY_BUFFER, resource->id);
            glBufferData(GL_ARRAY_BUFFER, params->data_size, params->data, GL_STATIC_DRAW);
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    return &resource->r;
}

static void gl_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct gl_resource *resource = gl_resource(res);

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UAV:
            if (res->desc.dimension == RESOURCE_DIMENSION_BUFFER)
            {
                glDeleteTextures(1, &resource->tbo_id);
                glDeleteBuffers(1, &resource->id);
            }
            else
            {
                glDeleteTextures(1, &resource->id);
            }
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            glDeleteBuffers(1, &resource->id);
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            break;
    }

    free(resource);
}

static uint32_t get_clip_distance_mask(const struct vkd3d_shader_signature *output)
{
    const struct vkd3d_shader_signature_element *e;
    uint32_t mask = 0;
    unsigned int i;

    for (i = 0; i < output->element_count; ++i)
    {
        e = &output->elements[i];
        if (e->sysval_semantic != VKD3D_SHADER_SV_CLIP_DISTANCE)
            continue;

        assert(e->semantic_index < 32u);
        mask |= 1u << e->semantic_index;
    }

    return mask;
}

static bool compile_shader(struct gl_runner *runner, enum shader_type shader_type,
        ID3DBlob *blob, struct vkd3d_shader_code *out)
{
    struct vkd3d_shader_spirv_target_info spirv_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
    struct vkd3d_shader_interface_info interface_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_INTERFACE_INFO};
    struct vkd3d_shader_compile_info info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
    struct vkd3d_shader_resource_binding bindings[MAX_RESOURCES + MAX_UNIFORM_BUFFERS];
    struct vkd3d_shader_scan_combined_resource_sampler_info combined_sampler_info;
    struct vkd3d_shader_scan_hull_shader_tessellation_info tessellation_info;
    struct vkd3d_shader_spirv_domain_shader_target_info domain_info;
    struct vkd3d_shader_combined_resource_sampler *sampler;
    struct vkd3d_shader_scan_signature_info signature_info;
    enum vkd3d_shader_spirv_extension spirv_extensions[1];
    struct vkd3d_shader_resource_binding *binding;
    struct vkd3d_shader_parameter parameters[1];
    unsigned int count, i;
    char *messages;
    int ret;

    const struct vkd3d_shader_compile_option options[] =
    {
        {VKD3D_SHADER_COMPILE_OPTION_API_VERSION, VKD3D_SHADER_API_VERSION_CURRENT},
        {VKD3D_SHADER_COMPILE_OPTION_FRAGMENT_COORDINATE_ORIGIN,
                VKD3D_SHADER_COMPILE_OPTION_FRAGMENT_COORDINATE_ORIGIN_LOWER_LEFT},
        {VKD3D_SHADER_COMPILE_OPTION_FEATURE, shader_runner_caps_get_feature_flags(&runner->caps)},
    };

    info.next = &combined_sampler_info;
    info.source.code = ID3D10Blob_GetBufferPointer(blob);
    info.source.size = ID3D10Blob_GetBufferSize(blob);
    info.source_type = VKD3D_SHADER_SOURCE_DXBC_TPF;
    info.target_type = runner->language == SPIR_V ? VKD3D_SHADER_TARGET_SPIRV_BINARY : VKD3D_SHADER_TARGET_GLSL;
    info.options = options;
    info.option_count = ARRAY_SIZE(options);
    info.log_level = VKD3D_SHADER_LOG_WARNING;

    combined_sampler_info.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_COMBINED_RESOURCE_SAMPLER_INFO;
    combined_sampler_info.next = &signature_info;

    signature_info.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_SIGNATURE_INFO;
    signature_info.next = &tessellation_info;

    tessellation_info.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_HULL_SHADER_TESSELLATION_INFO;
    tessellation_info.next = NULL;

    ret = vkd3d_shader_scan(&info, &messages);
    if (messages && vkd3d_test_state.debug_level)
        trace("%s\n", messages);
    vkd3d_shader_free_messages(messages);
    if (ret)
        return false;

    if (shader_type == SHADER_TYPE_HS)
    {
        runner->output_primitive = tessellation_info.output_primitive;
        runner->partitioning = tessellation_info.partitioning;
    }

    count = runner->combined_sampler_count + combined_sampler_info.combined_sampler_count;
    if (count && !(runner->combined_samplers = realloc(runner->combined_samplers,
            count * sizeof(*runner->combined_samplers))))
        fatal_error("Failed to allocate combined samplers array.\n");
    for (i = 0; i < combined_sampler_info.combined_sampler_count; ++i)
    {
        const struct vkd3d_shader_combined_resource_sampler_info *s = &combined_sampler_info.combined_samplers[i];

        sampler = &runner->combined_samplers[runner->combined_sampler_count];
        sampler->resource_space = s->resource_space;
        sampler->resource_index = s->resource_index;
        sampler->sampler_space = s->sampler_space;
        sampler->sampler_index = s->sampler_index;
        sampler->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        /* We don't know if this combined sampler was created from a SRV buffer or a SRV image, so
         * we pass both flags, otherwise the combined sampler won't be recognized when emitting the
         * SPIR-V, which will result in a failing assertion. */
        sampler->flags = VKD3D_SHADER_BINDING_FLAG_IMAGE | VKD3D_SHADER_BINDING_FLAG_BUFFER;
        sampler->binding.set = 0;
        sampler->binding.binding = runner->combined_sampler_count++;
        sampler->binding.count = 1;
    }
    vkd3d_shader_free_scan_combined_resource_sampler_info(&combined_sampler_info);

    runner->clip_distance_mask[shader_type] = get_clip_distance_mask(&signature_info.output);
    vkd3d_shader_free_scan_signature_info(&signature_info);

    if (runner->language == SPIR_V)
    {
        info.next = &spirv_info;
        spirv_info.next = &interface_info;
        spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_OPENGL_4_5;
        spirv_info.extensions = spirv_extensions;
        spirv_info.extension_count = 0;
        if (runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX])
            spirv_extensions[spirv_info.extension_count++] = VKD3D_SHADER_SPIRV_EXTENSION_EXT_VIEWPORT_INDEX_LAYER;
    }
    else
    {
        info.next = &interface_info;
    }

    if (shader_type == SHADER_TYPE_DS)
    {
        interface_info.next = &domain_info;

        domain_info.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_DOMAIN_SHADER_TARGET_INFO;
        domain_info.next = NULL;
        domain_info.output_primitive = runner->output_primitive;
        domain_info.partitioning = runner->partitioning;
    }

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        binding = &bindings[interface_info.binding_count++];
        binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_CBV;
        binding->register_space = 0;
        binding->register_index = i;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER;
        binding->binding.set = 0;
        binding->binding.binding = i;
        binding->binding.count = 1;
    }

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        const struct gl_resource *resource = gl_resource(runner->r.resources[i]);

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_UAV:
                binding = &bindings[interface_info.binding_count++];
                binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_UAV;
                binding->register_space = 0;
                binding->register_index = resource->r.desc.slot;
                binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
                if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
                    binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER;
                else
                    binding->flags = VKD3D_SHADER_BINDING_FLAG_IMAGE;
                binding->binding.set = 0;
                binding->binding.binding = resource->r.desc.slot;
                binding->binding.count = 1;
                break;

            default:
                break;
        }
    }

    interface_info.bindings = bindings;
    interface_info.combined_samplers = runner->combined_samplers;
    interface_info.combined_sampler_count = runner->combined_sampler_count;

    parameters[0].name = VKD3D_SHADER_PARAMETER_NAME_RASTERIZER_SAMPLE_COUNT;
    parameters[0].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[0].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[0].u.immediate_constant.u.u32 = runner->r.sample_count;

    spirv_info.parameter_count = ARRAY_SIZE(parameters);
    spirv_info.parameters = parameters;

    ret = vkd3d_shader_compile(&info, out, &messages);
    if (messages && vkd3d_test_state.debug_level)
        trace("%s\n", messages);
    vkd3d_shader_free_messages(messages);
    if (ret)
        return false;

    return true;
}

static void trace_info_log(GLuint id, bool program)
{
    const char *p, *end, *line;
    GLint length = 0;
    char *log;

    if (program)
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);
    else
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

    if (length <= 1)
        return;

    log = malloc(length);
    if (program)
        glGetProgramInfoLog(id, length, NULL, log);
    else
        glGetShaderInfoLog(id, length, NULL, log);
    log[length - 1] = '\n';

    trace("Info log received from %s #%u:\n", program ? "program" : "shader", id);

    p = log;
    end = &log[length];
    while (p < end)
    {
        line = p;
        if ((p = memchr(line, '\n', end - line)))
            ++p;
        else
            p = end;
        trace("    %.*s", (int)(p - line), line);
    }
    free(log);
}

static GLuint create_shader(struct gl_runner *runner, enum shader_type shader_type, ID3D10Blob *source)
{
    struct vkd3d_shader_code target;
    const GLchar *glsl_source;
    GLenum gl_shader_type;
    GLint status, size;
    const char *name;
    GLuint id;

    switch (shader_type)
    {
        case SHADER_TYPE_VS:
            gl_shader_type = GL_VERTEX_SHADER;
            name = "vertex";
            break;

        case SHADER_TYPE_PS:
            gl_shader_type = GL_FRAGMENT_SHADER;
            name = "fragment";
            break;

        case SHADER_TYPE_HS:
            gl_shader_type = GL_TESS_CONTROL_SHADER;
            name = "tessellation control";
            break;

        case SHADER_TYPE_DS:
            gl_shader_type = GL_TESS_EVALUATION_SHADER;
            name = "tessellation evaluation";
            break;

        case SHADER_TYPE_GS:
            gl_shader_type = GL_GEOMETRY_SHADER;
            name = "geometry";
            break;

        case SHADER_TYPE_CS:
            gl_shader_type = GL_COMPUTE_SHADER;
            name = "compute";
            break;

        default:
            fatal_error("Unhandled shader type %#x.\n", shader_type);
    }

    if (!compile_shader(runner, shader_type, source, &target))
        return 0;

    id = glCreateShader(gl_shader_type);
    if (runner->language == SPIR_V)
    {
        glShaderBinary(1, &id, GL_SHADER_BINARY_FORMAT_SPIR_V, target.code, target.size);
        p_glSpecializeShader(id, "main", 0, NULL, NULL);
    }
    else
    {
        glsl_source = target.code;
        size = target.size;
        glShaderSource(id, 1, &glsl_source, &size);
        glCompileShader(id);
    }
    vkd3d_shader_free_shader_code(&target);

    glGetShaderiv(id, GL_COMPILE_STATUS, &status);
    ok(status, "Failed to compile %s shader.\n", name);
    trace_info_log(id, false);

    return id;
}

static GLuint compile_compute_shader_program(struct gl_runner *runner)
{
    GLuint program_id, cs_id;
    ID3D10Blob *cs_blob;
    GLint status;

    reset_combined_samplers(runner);
    if (!(cs_blob = compile_hlsl(&runner->r, SHADER_TYPE_CS)))
        return 0;

    cs_id = create_shader(runner, SHADER_TYPE_CS, cs_blob);
    ID3D10Blob_Release(cs_blob);
    if (!cs_id)
        return 0;

    program_id = glCreateProgram();
    glAttachShader(program_id, cs_id);
    glLinkProgram(program_id);
    glGetProgramiv(program_id, GL_LINK_STATUS, &status);
    ok(status, "Failed to link program.\n");
    trace_info_log(program_id, true);

    glDeleteShader(cs_id);

    return program_id;
}

static bool gl_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    GLuint program_id, ubo_id[MAX_UNIFORM_BUFFERS];
    struct gl_runner *runner = gl_runner(r);
    unsigned int ubo_count, i;

    program_id = compile_compute_shader_program(runner);
    todo_if(runner->r.is_todo) ok(program_id, "Failed to compile shader program.\n");
    if (!program_id)
        return false;
    glUseProgram(program_id);

    for (i = 0, ubo_count = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        glGenBuffers(1, &ubo_id[ubo_count]);
        glBindBufferBase(GL_UNIFORM_BUFFER, i, ubo_id[ubo_count]);
        glBufferData(GL_UNIFORM_BUFFER, u->count * sizeof(*u->elements), u->elements, GL_STATIC_DRAW);
        ++ubo_count;
    }

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        struct gl_resource *resource = gl_resource(runner->r.resources[i]);

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
            case RESOURCE_TYPE_DEPTH_STENCIL:
            case RESOURCE_TYPE_VERTEX_BUFFER:
            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;

            case RESOURCE_TYPE_UAV:
                if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
                    glBindImageTexture(resource->r.desc.slot, resource->tbo_id, 0, GL_TRUE,
                            0, GL_READ_WRITE, resource->format->internal_format);
                else
                    glBindImageTexture(resource->r.desc.slot, resource->id, 0, GL_TRUE,
                            0, GL_READ_WRITE, resource->format->internal_format);
                break;
        }
    }

    glDispatchCompute(x, y, z);

    if (ubo_count)
        glDeleteBuffers(ubo_count, ubo_id);
    glDeleteProgram(program_id);

    return true;
}

static GLenum get_topology_gl(D3D_PRIMITIVE_TOPOLOGY topology)
{
    switch (topology)
    {
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
            return GL_POINTS;

        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            return GL_TRIANGLES;

        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
            return GL_TRIANGLE_STRIP;

        default:
            if (topology >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
                    && topology <= D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST)
                return GL_PATCHES;
            fatal_error("Unhandled topology %#x.\n", topology);
    }
}

static GLenum get_texture_wrap_gl(D3D12_TEXTURE_ADDRESS_MODE mode)
{
    switch (mode)
    {
        case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
            return GL_REPEAT;

        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
            return GL_MIRRORED_REPEAT;

        case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:
            return GL_CLAMP_TO_EDGE;

        case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
            return GL_CLAMP_TO_BORDER_ARB;

        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE:
            return GL_MIRROR_CLAMP_TO_EDGE;

        default:
            fatal_error("Unhandled address mode %#x.\n", mode);
    }
}

static GLenum get_texture_filter_mag_gl(D3D12_FILTER filter)
{
    return filter & 0x4 ? GL_LINEAR : GL_NEAREST;
}

static GLenum get_texture_filter_min_gl(D3D12_FILTER filter)
{
    bool min_linear = filter & 0x10, mip_linear = filter & 0x1;

    if (min_linear)
        return mip_linear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
    else
        return mip_linear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
}

static GLenum get_compare_op_gl(D3D12_COMPARISON_FUNC op)
{
    switch (op)
    {
        case D3D12_COMPARISON_FUNC_NEVER:
            return GL_NEVER;
        case D3D12_COMPARISON_FUNC_LESS:
            return GL_LESS;
        case D3D12_COMPARISON_FUNC_EQUAL:
            return GL_EQUAL;
        case D3D12_COMPARISON_FUNC_LESS_EQUAL:
            return GL_LEQUAL;
        case D3D12_COMPARISON_FUNC_GREATER:
            return GL_GREATER;
        case D3D12_COMPARISON_FUNC_NOT_EQUAL:
            return GL_NOTEQUAL;
        case D3D12_COMPARISON_FUNC_GREATER_EQUAL:
            return GL_GEQUAL;
        case D3D12_COMPARISON_FUNC_ALWAYS:
            return GL_ALWAYS;
        default:
            fatal_error("Unhandled compare op %#x.\n", op);
    }
}

static GLenum get_texture_reduction_mode_gl(D3D12_FILTER filter)
{
    D3D12_FILTER_REDUCTION_TYPE r = D3D12_DECODE_FILTER_REDUCTION(filter);

    switch (r)
    {
        case D3D12_FILTER_REDUCTION_TYPE_STANDARD:
        case D3D12_FILTER_REDUCTION_TYPE_COMPARISON:
            return GL_WEIGHTED_AVERAGE_ARB;
        case D3D12_FILTER_REDUCTION_TYPE_MINIMUM:
            return GL_MIN;
        case D3D12_FILTER_REDUCTION_TYPE_MAXIMUM:
            return GL_MAX;
        default:
            fatal_error("Unhandled reduction type %#x.\n", r);
    }
}

static GLuint compile_graphics_shader_program(struct gl_runner *runner, ID3D10Blob **vs_blob)
{
    ID3D10Blob *fs_blob, *hs_blob = NULL, *ds_blob = NULL, *gs_blob = NULL;
    GLuint program_id, vs_id, fs_id, hs_id = 0, ds_id = 0, gs_id = 0;
    bool succeeded;
    GLint status;

    reset_combined_samplers(runner);

    *vs_blob = compile_hlsl(&runner->r, SHADER_TYPE_VS);
    fs_blob = compile_hlsl(&runner->r, SHADER_TYPE_PS);
    succeeded = *vs_blob && fs_blob;

    if (runner->r.shader_source[SHADER_TYPE_HS])
    {
        hs_blob = compile_hlsl(&runner->r, SHADER_TYPE_HS);
        succeeded = succeeded && hs_blob;
    }
    if (runner->r.shader_source[SHADER_TYPE_DS])
    {
        ds_blob = compile_hlsl(&runner->r, SHADER_TYPE_DS);
        succeeded = succeeded && ds_blob;
    }
    if (runner->r.shader_source[SHADER_TYPE_GS])
    {
        gs_blob = compile_hlsl(&runner->r, SHADER_TYPE_GS);
        succeeded = succeeded && gs_blob;
    }

    if (!succeeded)
        goto fail;

    if (!(vs_id = create_shader(runner, SHADER_TYPE_VS, *vs_blob)))
        goto fail;

    if (!(fs_id = create_shader(runner, SHADER_TYPE_PS, fs_blob)))
        goto fail;
    ID3D10Blob_Release(fs_blob);
    fs_blob = NULL;

    if (hs_blob)
    {
        if (!(hs_id = create_shader(runner, SHADER_TYPE_HS, hs_blob)))
            goto fail;
        ID3D10Blob_Release(hs_blob);
        hs_blob = NULL;
    }

    if (ds_blob)
    {
        if (!(ds_id = create_shader(runner, SHADER_TYPE_DS, ds_blob)))
            goto fail;
        ID3D10Blob_Release(ds_blob);
        ds_blob = NULL;
    }

    if (gs_blob)
    {
        if (!(gs_id = create_shader(runner, SHADER_TYPE_GS, gs_blob)))
            goto fail;
        ID3D10Blob_Release(gs_blob);
        gs_blob = NULL;
    }

    /* TODO: compile and use the gs blobs too, but currently this
     * point is not reached because compile_hlsl() fails on these. */

    program_id = glCreateProgram();
    glAttachShader(program_id, vs_id);
    glAttachShader(program_id, fs_id);
    if (hs_id)
        glAttachShader(program_id, hs_id);
    if (ds_id)
        glAttachShader(program_id, ds_id);
    if (gs_id)
        glAttachShader(program_id, gs_id);
    glLinkProgram(program_id);
    glGetProgramiv(program_id, GL_LINK_STATUS, &status);
    ok(status, "Failed to link program.\n");
    trace_info_log(program_id, true);

    if (gs_id)
        glDeleteShader(gs_id);
    if (ds_id)
        glDeleteShader(ds_id);
    if (hs_id)
        glDeleteShader(hs_id);
    glDeleteShader(fs_id);
    glDeleteShader(vs_id);

    return program_id;

fail:
    if (gs_blob)
        ID3D10Blob_Release(gs_blob);
    if (ds_blob)
        ID3D10Blob_Release(ds_blob);
    if (hs_blob)
        ID3D10Blob_Release(hs_blob);
    if (fs_blob)
        ID3D10Blob_Release(fs_blob);
    if (*vs_blob)
        ID3D10Blob_Release(*vs_blob);
    return 0;
}

static void gl_runner_clear(struct shader_runner *r, struct resource *res, const struct vec4 *clear_value)
{
    struct gl_resource *resource = gl_resource(res);
    struct gl_runner *runner = gl_runner(r);
    GLbitfield clear_mask;

    if (!runner->fbo_id)
        glGenFramebuffers(1, &runner->fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, runner->fbo_id);

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, resource->id, 0);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(clear_value->x, clear_value->y, clear_value->z, clear_value->w);
            clear_mask = GL_COLOR_BUFFER_BIT;
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, resource->id, 0);
            glDepthMask(GL_TRUE);
            glClearDepthf(clear_value->x);
            clear_mask = GL_DEPTH_BUFFER_BIT;
            break;

        default:
            fatal_error("Clears are not implemented for resource type %u.\n", resource->r.desc.type);
    }

    glScissor(0, 0, res->desc.width, res->desc.height);
    glClear(clear_mask);
}

static bool gl_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    unsigned int attribute_idx, fb_width, fb_height, rt_count, ubo_count, i, j;
    struct vkd3d_shader_signature vs_input_signature;
    GLuint program_id, ubo_id[MAX_UNIFORM_BUFFERS];
    struct gl_runner *runner = gl_runner(r);
    struct vkd3d_shader_code vs_dxbc;
    uint32_t clip_distance_mask, map;
    uint8_t *attribute_offsets[32];
    struct
    {
        GLuint id;
        GLsizei stride;
    } vbo_info[MAX_RESOURCES];
    GLint max_clip_distance_count;
    ID3D10Blob *vs_blob;
    int ret;
    struct
    {
        GLuint id;
    } sampler_info[MAX_SAMPLERS];
    GLenum draw_buffers[8];

    program_id = compile_graphics_shader_program(runner, &vs_blob);
    todo_if(runner->r.is_todo) ok(program_id, "Failed to compile shader program.\n");
    if (!program_id)
        return false;
    glUseProgram(program_id);

    for (i = 0, ubo_count = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        glGenBuffers(1, &ubo_id[ubo_count]);
        glBindBufferBase(GL_UNIFORM_BUFFER, i, ubo_id[ubo_count]);
        glBufferData(GL_UNIFORM_BUFFER, u->count * sizeof(*u->elements), u->elements, GL_STATIC_DRAW);
        ++ubo_count;
    }

    if (!runner->fbo_id)
        glGenFramebuffers(1, &runner->fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, runner->fbo_id);

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        struct sampler *sampler = &runner->r.samplers[i];
        GLuint id;

        glGenSamplers(1, &id);
        glSamplerParameteri(id, GL_TEXTURE_WRAP_S, get_texture_wrap_gl(sampler->u_address));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_T, get_texture_wrap_gl(sampler->v_address));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_R, get_texture_wrap_gl(sampler->w_address));
        glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, get_texture_filter_mag_gl(sampler->filter));
        glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, get_texture_filter_min_gl(sampler->filter));
        if (runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX])
            glSamplerParameteri(id, GL_TEXTURE_REDUCTION_MODE_ARB, get_texture_reduction_mode_gl(sampler->filter));
        if (sampler->func)
        {
            glSamplerParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(id, GL_TEXTURE_COMPARE_FUNC, get_compare_op_gl(sampler->func));
        }
        sampler_info[i].id = id;
    }

    for (i = 0; i < runner->combined_sampler_count; ++i)
    {
        const struct vkd3d_shader_combined_resource_sampler *s = &runner->combined_samplers[i];
        struct resource *resource;
        struct sampler *sampler;

        if (s->resource_space || s->sampler_space)
            fatal_error("Unsupported register space.\n");

        if (!(resource = shader_runner_get_resource(r, RESOURCE_TYPE_TEXTURE, s->resource_index)))
            fatal_error("Resource not found.\n");

        glActiveTexture(GL_TEXTURE0 + s->binding.binding);
        if (resource->desc.dimension == RESOURCE_DIMENSION_BUFFER)
            glBindTexture(gl_resource(resource)->target, gl_resource(resource)->tbo_id);
        else
            glBindTexture(gl_resource(resource)->target, gl_resource(resource)->id);

        if (s->sampler_index == VKD3D_SHADER_DUMMY_SAMPLER_INDEX)
            continue;

        if (!(sampler = shader_runner_get_sampler(r, s->sampler_index)))
            fatal_error("Sampler not found.\n");
        glBindSampler(s->binding.binding, sampler_info[sampler - r->samplers].id);
    }

    fb_width = ~0u;
    fb_height = ~0u;
    memset(vbo_info, 0, sizeof(vbo_info));
    memset(draw_buffers, 0, sizeof(draw_buffers));
    for (i = 0, rt_count = 0; i < runner->r.resource_count; ++i)
    {
        struct gl_resource *resource = gl_resource(runner->r.resources[i]);

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
                glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + resource->r.desc.slot, resource->id, 0);
                if (resource->r.desc.slot >= ARRAY_SIZE(draw_buffers))
                    fatal_error("Unsupported render target index %u.\n", resource->r.desc.slot);
                draw_buffers[resource->r.desc.slot] = GL_COLOR_ATTACHMENT0 + resource->r.desc.slot;
                if (resource->r.desc.slot >= rt_count)
                    rt_count = resource->r.desc.slot + 1;
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_DEPTH_STENCIL:
                glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, resource->id, 0);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                glDepthFunc(get_compare_op_gl(runner->r.depth_func));
                if (runner->r.depth_bounds)
                {
                    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
                    p_glDepthBoundsEXT(runner->r.depth_min, runner->r.depth_max);
                }
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;

            case RESOURCE_TYPE_UAV:
                if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
                {
                    glBindImageTexture(resource->r.desc.slot, resource->tbo_id, 0, GL_TRUE,
                            0, GL_READ_WRITE, resource->format->internal_format);
                }
                else
                {
                    glBindImageTexture(resource->r.desc.slot, resource->id, 0, GL_TRUE,
                            0, GL_READ_WRITE, resource->format->internal_format);
                }
                break;

            case RESOURCE_TYPE_VERTEX_BUFFER:
                assert(resource->r.desc.slot < ARRAY_SIZE(vbo_info));
                vbo_info[resource->r.desc.slot].id = resource->id;
                for (j = 0; j < runner->r.input.element_count; ++j)
                {
                    if (runner->r.input.elements[j].slot != resource->r.desc.slot)
                        continue;
                    assert(j < ARRAY_SIZE(attribute_offsets));
                    attribute_offsets[j] = (uint8_t *)(uintptr_t)vbo_info[resource->r.desc.slot].stride;
                    vbo_info[resource->r.desc.slot].stride += runner->r.input.elements[j].texel_size;
                }
                break;
        }
    }

    glEnable(GL_SAMPLE_MASK);
    glSampleMaski(0, runner->r.sample_mask);

    if (r->viewport_count)
    {
        for (i = 0; i < r->viewport_count; ++i)
        {
            glViewportIndexedf(i, r->viewports[i].x, r->viewports[i].y, r->viewports[i].width, r->viewports[i].height);
        }
    }
    else
    {
        glViewport(0, 0, fb_width, fb_height);
    }
    glScissor(0, 0, fb_width, fb_height);
    glDrawBuffers(rt_count, draw_buffers);

    vs_dxbc.code = ID3D10Blob_GetBufferPointer(vs_blob);
    vs_dxbc.size = ID3D10Blob_GetBufferSize(vs_blob);
    ret = vkd3d_shader_parse_input_signature(&vs_dxbc, &vs_input_signature, NULL);
    ok(!ret, "Failed to parse input signature, error %d.\n", ret);

    map = runner->attribute_map;
    for (i = 0, runner->attribute_map = 0; i < runner->r.input.element_count; ++i)
    {
        const struct input_element *element = &runner->r.input.elements[i];
        const struct vkd3d_shader_signature_element *signature_element;
        const struct format_info *format;

        if (!(signature_element = vkd3d_shader_find_signature_element(&vs_input_signature,
                element->name, element->index, 0)))
            continue;
        attribute_idx = signature_element->register_index;
        format = get_format_info(element->format, false);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_info[element->slot].id);
        if (format->is_integer)
            glVertexAttribIPointer(attribute_idx, format->component_count, format->type,
                    vbo_info[element->slot].stride, attribute_offsets[i]);
        else
            glVertexAttribPointer(attribute_idx, format->component_count, format->type,
                    GL_FALSE, vbo_info[element->slot].stride, attribute_offsets[i]);
        glEnableVertexAttribArray(attribute_idx);
        runner->attribute_map |= attribute_idx;
    }
    vkd3d_shader_free_shader_signature(&vs_input_signature);

    for (i = 0, clip_distance_mask = 0; i < SHADER_TYPE_COUNT; ++i)
    {
        clip_distance_mask |= runner->clip_distance_mask[i];
    }

    glGetIntegerv(GL_MAX_CLIP_DISTANCES, &max_clip_distance_count);
    if (clip_distance_mask & ~((1u << max_clip_distance_count) - 1))
        trace("Warning: Not enough clip distances supported (maximum is %d).\n", max_clip_distance_count);

    /* OpenGL expects all declared gl_ClipDistance[] arrays to be the same
     * size across shaders. */
    for (i = 0; i < SHADER_TYPE_COUNT; ++i)
    {
        if (!runner->clip_distance_mask[i])
            continue;
        if (runner->clip_distance_mask[i] != clip_distance_mask)
            trace("Warning: Shader type %#x has clip distance mask %#x (subset of the combined %#x).\n",
                    i, runner->clip_distance_mask[i], clip_distance_mask);
    }

    for (i = 0; i < max_clip_distance_count; ++i)
    {
        if (clip_distance_mask & (1u << i))
            glEnable(GL_CLIP_DISTANCE0 + i);
        else
            glDisable(GL_CLIP_DISTANCE0 + i);
    }

    map &= ~runner->attribute_map;
    for (attribute_idx = 0; map; ++attribute_idx, map >>= 1)
    {
        if (map & 1)
            glDisableVertexAttribArray(attribute_idx);
    }

    if (runner->r.shader_source[SHADER_TYPE_HS])
        glPatchParameteri(GL_PATCH_VERTICES, max(topology - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1, 1));

    p_glDrawArraysInstancedBaseInstance(get_topology_gl(topology),
            first_vertex, vertex_count, instance_count, first_instance);

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        glDeleteSamplers(1, &sampler_info[i].id);
    }
    if (ubo_count)
        glDeleteBuffers(ubo_count, ubo_id);

    ID3D10Blob_Release(vs_blob);
    glDeleteProgram(program_id);

    return true;
}

static bool gl_runner_copy(struct shader_runner *r, struct resource *src, struct resource *dst)
{
    struct gl_resource *s = gl_resource(src);
    struct gl_resource *d = gl_resource(dst);
    unsigned int l, w, h, z;

    if (src->desc.dimension == RESOURCE_DIMENSION_BUFFER || src->desc.layer_count > 1)
        return false;

    for (l = 0; l < src->desc.level_count; ++l)
    {
        w = get_level_dimension(src->desc.width, l);
        h = get_level_dimension(src->desc.height, l);
        z = get_level_dimension(src->desc.depth, l);
        glCopyImageSubData(s->id, s->target, l, 0, 0, 0, d->id, d->target, l, 0, 0, 0, w, h, z);
    }

    return true;
}

struct gl_resource_readback
{
    struct resource_readback rb;
};

static struct resource_readback *gl_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct gl_resource *resource = gl_resource(res);
    struct gl_runner *runner = gl_runner(r);
    struct resource_readback *rb;
    unsigned int layer, level;
    size_t slice_pitch;

    if (resource->r.desc.type != RESOURCE_TYPE_RENDER_TARGET && resource->r.desc.type != RESOURCE_TYPE_DEPTH_STENCIL
            && resource->r.desc.type != RESOURCE_TYPE_UAV)
        fatal_error("Unhandled resource type %#x.\n", resource->r.desc.type);

    rb = malloc(sizeof(*rb));

    rb->width = resource->r.desc.width;
    rb->height = resource->r.desc.height;
    rb->depth = resource->r.desc.depth;

    rb->row_pitch = rb->width * resource->r.desc.texel_size;
    slice_pitch = rb->row_pitch * rb->height;
    rb->data = calloc(slice_pitch, resource->r.desc.depth);

    level = sub_resource_idx % resource->r.desc.level_count;
    layer = sub_resource_idx / resource->r.desc.level_count;

    if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
    {
        glBindBuffer(resource->target, resource->id);
        glGetBufferSubData(resource->target, 0, slice_pitch, rb->data);
    }
    else if (resource->r.desc.sample_count > 1)
    {
        GLuint src_fbo, dst_fbo;
        GLuint resolved;

        glGenTextures(1, &resolved);
        glBindTexture(GL_TEXTURE_2D, resolved);
        glTexStorage2D(GL_TEXTURE_2D, resource->r.desc.level_count,
                resource->format->internal_format, resource->r.desc.width, resource->r.desc.height);

        glGenFramebuffers(1, &src_fbo);
        glGenFramebuffers(1, &dst_fbo);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);

        glFramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, resource->id, 0);
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, resolved, 0);

        glBlitFramebuffer(0, 0, resource->r.desc.width, resource->r.desc.height,
                0, 0, resource->r.desc.width, resource->r.desc.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, runner->fbo_id);
        glDeleteFramebuffers(1, &src_fbo);
        glDeleteFramebuffers(1, &dst_fbo);

        glGetTexImage(GL_TEXTURE_2D, 0, resource->format->format, resource->format->type, rb->data);

        glDeleteTextures(1, &resolved);
    }
    else
    {
        glBindTexture(resource->target, resource->id);
        glGetTexImage(resource->target, level, resource->format->format, resource->format->type, rb->data);
        if (layer)
            memcpy(rb->data, (const uint8_t *)rb->data + layer * slice_pitch, slice_pitch);
    }

    return rb;
}

static void gl_runner_release_readback(struct shader_runner *runner, struct resource_readback *rb)
{
    free(rb->data);
    free(rb);
}

static const struct shader_runner_ops gl_runner_ops =
{
    .create_resource = gl_runner_create_resource,
    .destroy_resource = gl_runner_destroy_resource,
    .dispatch = gl_runner_dispatch,
    .clear = gl_runner_clear,
    .draw = gl_runner_draw,
    .copy = gl_runner_copy,
    .get_resource_readback = gl_runner_get_resource_readback,
    .release_readback = gl_runner_release_readback,
};

static void run_tests(enum shading_language language)
{
    struct gl_runner runner;

    if (test_skipping_execution(language == SPIR_V ? "OpenGL/SPIR-V" : "OpenGL/GLSL",
            HLSL_COMPILER, SHADER_MODEL_4_0, SHADER_MODEL_5_1))
        return;

    if (!gl_runner_init(&runner, language))
        return;
    run_shader_tests(&runner.r, &runner.caps, &gl_runner_ops, NULL);
    gl_runner_cleanup(&runner);
}

void run_shader_tests_gl(void)
{
    run_tests(SPIR_V);
    run_tests(GLSL);
}

#endif
