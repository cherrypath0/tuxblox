/*
 * Shader runner which uses libvkd3d-shader to compile HLSL -> D3D bytecode -> SPIR-V
 *
 * Copyright 2020-2024 Elizabeth Figura for CodeWeavers
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

#ifndef _WIN32

#define COBJMACROS
#define VK_NO_PROTOTYPES
#define VKD3D_TEST_NO_DEFS
#include "config.h"
#include "vkd3d.h"
#include "vkd3d_d3dcompiler.h"
#include "shader_runner.h"
#include "vulkan_utils.h"
#include "vkd3d_test.h"

#define LOG2_E 1.44269504f
#define SQRT_LOG2_E 1.20112241f

struct vulkan_resource
{
    struct resource r;

    VkBuffer buffer;
    VkBufferView buffer_view;
    VkImage image;
    VkImageView image_view;
    VkDeviceMemory memory;
};

static struct vulkan_resource *vulkan_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct vulkan_resource, r);
}

struct vulkan_shader_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;
    bool demote_to_helper_invocation;
    bool driver_properties;

    struct vulkan_test_context context;

    ID3D10Blob *d3d_blobs[SHADER_TYPE_COUNT];
    struct vkd3d_shader_scan_signature_info signatures[SHADER_TYPE_COUNT];
    VkExtent2D rt_size;

    struct vulkan_sampler
    {
        VkSampler vk_sampler;
        uint32_t binding;
    } samplers[MAX_SAMPLERS];
};

struct physical_device_info
{
    VkPhysicalDeviceFeatures2 features2;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlock_features;
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demote_to_helper_invocation_features;

    VkPhysicalDeviceProperties2 properties2;
    VkPhysicalDeviceDriverPropertiesKHR driver_properties;
    VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT filter_minmax_properties;
    VkPhysicalDeviceSubgroupProperties subgroup_properties;
};

static struct vulkan_shader_runner *vulkan_shader_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct vulkan_shader_runner, r);
}

static void resource_init_texture(struct vulkan_shader_runner *runner, struct vulkan_resource *resource,
        const struct resource_params *params)
{
    VkImageUsageFlagBits usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const struct vulkan_test_context *context = &runner->context;
    VkFormat format = vkd3d_get_vk_format(params->desc.format);
    const struct resource_desc *desc = &params->desc;
    uint32_t layer_count = desc->layer_count;
    VkDevice device = context->device;
    unsigned int buffer_offset = 0;
    VkDeviceMemory staging_memory;
    VkImageCreateFlags flags = 0;
    VkBuffer staging_buffer;
    VkImageType image_type;
    float min_lod_clamp;
    void *data;

    if (params->desc.type == RESOURCE_TYPE_UAV)
    {
        layout = VK_IMAGE_LAYOUT_GENERAL;
        usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (params->desc.dimension == RESOURCE_DIMENSION_3D)
        image_type = VK_IMAGE_TYPE_3D;
    else
        image_type = VK_IMAGE_TYPE_2D;

    if (params->desc.dimension == RESOURCE_DIMENSION_CUBE)
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    min_lod_clamp = runner->caps.shader_caps[SHADER_CAP_MIN_LOD_CLAMP] ? desc->min_lod_clamp : 0.0f;

    resource->image = create_vulkan_image(context, image_type, desc->width, desc->height, desc->depth,
            desc->level_count, desc->layer_count, desc->sample_count, usage, format, flags, &resource->memory);
    resource->image_view = create_vulkan_image_view(context,
            resource->image, format, VK_IMAGE_ASPECT_COLOR_BIT, image_type,
            (params->desc.dimension == RESOURCE_DIMENSION_CUBE), layer_count, min_lod_clamp);

    if (!params->data)
    {
        begin_command_buffer(context);
        transition_image_layout(context, resource->image, VK_IMAGE_ASPECT_COLOR_BIT,
                0, layer_count, VK_IMAGE_LAYOUT_UNDEFINED, layout);
        end_command_buffer(context);
        return;
    }

    staging_buffer = create_vulkan_buffer(context, params->data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_memory);
    VK_CALL(vkMapMemory(device, staging_memory, 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, params->data, params->data_size);
    VK_CALL(vkUnmapMemory(device, staging_memory));

    begin_command_buffer(context);

    transition_image_layout(context, resource->image, VK_IMAGE_ASPECT_COLOR_BIT, 0,
            layer_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    for (unsigned int level = 0; level < params->desc.level_count; ++level)
    {
        unsigned int level_width = get_level_dimension(params->desc.width, level);
        unsigned int level_height = get_level_dimension(params->desc.height, level);
        unsigned int level_depth = get_level_dimension(params->desc.depth, level);

        for (unsigned int layer = 0; layer < params->desc.layer_count; ++layer)
        {
            VkBufferImageCopy region = {0};

            region.bufferOffset = buffer_offset;
            region.imageSubresource.mipLevel = level;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = level_width;
            region.imageExtent.height = level_height;
            region.imageExtent.depth = level_depth;
            VK_CALL(vkCmdCopyBufferToImage(context->cmd_buffer, staging_buffer, resource->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));

            buffer_offset += level_depth * level_width * level_height * params->desc.texel_size;
        }
    }

    transition_image_layout(context, resource->image, VK_IMAGE_ASPECT_COLOR_BIT,
            0, layer_count, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

    end_command_buffer(context);

    VK_CALL(vkFreeMemory(device, staging_memory, NULL));
    VK_CALL(vkDestroyBuffer(device, staging_buffer, NULL));
}

static void resource_init_buffer(struct vulkan_shader_runner *runner, struct vulkan_resource *resource,
        const struct resource_params *params)
{
    const struct vulkan_test_context *context = &runner->context;
    VkFormat format = vkd3d_get_vk_format(params->desc.format);
    VkDevice device = context->device;
    VkBufferUsageFlagBits usage;
    void *data;

    if (params->desc.type == RESOURCE_TYPE_UAV)
        usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    else
        usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

    /* d3d12 requires DXGI_FORMAT_UNKNOWN for structured buffers, but Vulkan requires a defined format. */
    if (format == VK_FORMAT_UNDEFINED && params->stride)
        format = VK_FORMAT_R32_UINT;

    resource->buffer = create_vulkan_buffer(context, params->data_size, usage,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &resource->memory);
    resource->buffer_view = create_vulkan_buffer_view(context, resource->buffer, format, 0);

    VK_CALL(vkMapMemory(device, resource->memory, 0, VK_WHOLE_SIZE, 0, &data));
    memcpy(data, params->data, params->data_size);
    VK_CALL(vkUnmapMemory(device, resource->memory));
}

static struct resource *vulkan_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    const struct resource_desc *desc = &params->desc;
    unsigned int layer_count = desc->layer_count;
    VkDevice device = context->device;
    struct vulkan_resource *resource;
    VkFormat format;
    void *data;

    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            format = vkd3d_get_vk_format(params->desc.format);

            resource->image = create_vulkan_image(context, VK_IMAGE_TYPE_2D,
                    desc->width, desc->height, 1, desc->level_count, desc->layer_count, desc->sample_count,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    format, 0, &resource->memory);
            resource->image_view = create_vulkan_image_view(context,
                    resource->image, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_TYPE_2D, false, layer_count, 0.0f);

            begin_command_buffer(context);
            transition_image_layout(context, resource->image, VK_IMAGE_ASPECT_COLOR_BIT, 0,
                    layer_count, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            end_command_buffer(context);
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            format = vkd3d_get_vk_format(params->desc.format);

            resource->image = create_vulkan_image(context, VK_IMAGE_TYPE_2D,
                    desc->width, desc->height, 1, desc->level_count, desc->layer_count, desc->sample_count,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    format, 0, &resource->memory);
            resource->image_view = create_vulkan_image_view(context,
                    resource->image, format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_TYPE_2D, false, layer_count, 0.0f);

            begin_command_buffer(context);
            transition_image_layout(context, resource->image, VK_IMAGE_ASPECT_DEPTH_BIT, 0, layer_count,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            end_command_buffer(context);
            break;

        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UAV:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
                resource_init_buffer(runner, resource, params);
            else
                resource_init_texture(runner, resource, params);
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            resource->buffer = create_vulkan_buffer(context, params->data_size,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &resource->memory);

            VK_CALL(vkMapMemory(device, resource->memory, 0, VK_WHOLE_SIZE, 0, &data));
            memcpy(data, params->data, params->data_size);
            VK_CALL(vkUnmapMemory(device, resource->memory));
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    return &resource->r;
}

static void vulkan_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    struct vulkan_resource *resource = vulkan_resource(res);
    VkDevice device = context->device;

    if (resource->memory)
        VK_CALL(vkFreeMemory(device, resource->memory, NULL));
    if (resource->image)
        VK_CALL(vkDestroyImage(device, resource->image, NULL));
    if (resource->image_view)
        VK_CALL(vkDestroyImageView(device, resource->image_view, NULL));
    if (resource->buffer)
        VK_CALL(vkDestroyBuffer(device, resource->buffer, NULL));
    if (resource->buffer_view)
        VK_CALL(vkDestroyBufferView(device, resource->buffer_view, NULL));

    free(resource);
}

static struct vulkan_resource *find_resource(struct vulkan_shader_runner *runner,
        const struct descriptor_mapping *descriptor, unsigned int offset)
{
    enum resource_type resource_type;
    struct resource *resource;

    switch (descriptor->type)
    {
        case VKD3D_SHADER_DESCRIPTOR_TYPE_SRV:
            resource_type = RESOURCE_TYPE_TEXTURE;
            break;

        case VKD3D_SHADER_DESCRIPTOR_TYPE_UAV:
            resource_type = RESOURCE_TYPE_UAV;
            break;

        default:
            fatal_error("Unhandled descriptor type %#x.\n", descriptor->type);
    }

    if (!(resource = shader_runner_get_resource(&runner->r, resource_type, descriptor->target_idx + offset)))
        return NULL;

    return vulkan_resource(resource);
}

static bool is_mapping_buffer(struct vulkan_shader_runner *runner, const struct descriptor_mapping *descriptor)
{
    bool is_buffer = false, found = false;
    unsigned int i;

    for (i = 0; i < descriptor->count; ++i)
    {
        const struct vulkan_resource *resource;

        if (!(resource = find_resource(runner, descriptor, i)))
            continue;

        if (!found)
        {
            is_buffer = resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER;
            found = true;
            continue;
        }

        if (is_buffer != (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER))
            fatal_error("Descriptor binding with mixed buffer and non-buffer resources.\n");
    }

    /* It might be that we found no descriptor at all; in that case it doesn't
     * matter what we return. */
    return is_buffer;
}

static enum vkd3d_shader_fog_fragment_mode get_fog_fragment_mode(enum fog_mode mode)
{
    switch (mode)
    {
        case FOG_MODE_DISABLE: return VKD3D_SHADER_FOG_FRAGMENT_NONE;
        case FOG_MODE_NONE: return VKD3D_SHADER_FOG_FRAGMENT_LINEAR;
        case FOG_MODE_LINEAR: return VKD3D_SHADER_FOG_FRAGMENT_LINEAR;
        case FOG_MODE_EXP: return VKD3D_SHADER_FOG_FRAGMENT_EXP;
        case FOG_MODE_EXP2: return VKD3D_SHADER_FOG_FRAGMENT_EXP2;
        default: fatal_error("Unhandled fog mode %#x.\n", mode);
    }
}

static enum vkd3d_shader_fog_source get_fog_source(const struct shader_runner *runner)
{
    if (runner->fog_mode == FOG_MODE_DISABLE)
        return VKD3D_SHADER_FOG_SOURCE_FOG;
    else if (runner->fog_mode == FOG_MODE_NONE)
        return VKD3D_SHADER_FOG_SOURCE_FOG_OR_SPECULAR_W;

    return runner->ortho_fog ? VKD3D_SHADER_FOG_SOURCE_Z : VKD3D_SHADER_FOG_SOURCE_W;
}

static void free_d3d_blobs(struct vulkan_shader_runner *runner)
{
    unsigned int i;

    for (i = 0; i < SHADER_TYPE_COUNT; ++i)
    {
        if (!runner->d3d_blobs[i])
            continue;

        vkd3d_shader_free_scan_signature_info(&runner->signatures[i]);
        ID3D10Blob_Release(runner->d3d_blobs[i]);
        runner->d3d_blobs[i] = NULL;
    }
}

static bool compile_hlsl_and_scan(struct vulkan_shader_runner *runner, enum shader_type type)
{
    struct vkd3d_shader_parameter_info parameter_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_PARAMETER_INFO};
    struct vkd3d_shader_compile_info info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
    struct vkd3d_shader_parameter1 parameters[1];
    enum vkd3d_result ret;

    if (!(runner->d3d_blobs[type] = compile_hlsl(&runner->r, type)))
        return false;

    info.next = &parameter_info;
    info.source.code = ID3D10Blob_GetBufferPointer(runner->d3d_blobs[type]);
    info.source.size = ID3D10Blob_GetBufferSize(runner->d3d_blobs[type]);
    if (runner->r.shader_model < SHADER_MODEL_4_0)
        info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
    else if (runner->r.minimum_shader_model < SHADER_MODEL_6_0)
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_TPF;
    else
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_DXIL;
    info.target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY;

    parameter_info.next = &runner->signatures[type];
    parameter_info.parameter_count = ARRAY_SIZE(parameters);
    parameter_info.parameters = parameters;

    parameters[0].name = VKD3D_SHADER_PARAMETER_NAME_FOG_FRAGMENT_MODE;
    parameters[0].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[0].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[0].u.immediate_constant.u.u32 = get_fog_fragment_mode(runner->r.fog_mode);

    runner->signatures[type].type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_SIGNATURE_INFO;
    runner->signatures[type].next = NULL;
    if ((ret = vkd3d_shader_scan(&info, NULL)) < 0)
    {
        ID3D10Blob_Release(runner->d3d_blobs[type]);
        runner->d3d_blobs[type] = NULL;
        return false;
    }

    return true;
}

static bool compile_d3d_code(struct vulkan_shader_runner *runner,
        enum shader_type type, struct vkd3d_shader_code *spirv)
{
    struct vkd3d_shader_d3dbc_source_info d3dbc_source_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_D3DBC_SOURCE_INFO};
    struct vkd3d_shader_varying_map_info varying_map_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_VARYING_MAP_INFO};
    struct vkd3d_shader_spirv_target_info spirv_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
    struct vkd3d_shader_interface_info interface_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_INTERFACE_INFO};
    struct vkd3d_shader_parameter_info parameter_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_PARAMETER_INFO};
    struct vkd3d_shader_compile_info info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
    struct vkd3d_shader_push_constant_buffer push_constants[MAX_UNIFORM_BUFFERS];
    struct vkd3d_shader_resource_binding bindings[MAX_RESOURCES + MAX_SAMPLERS];
    enum vkd3d_shader_spirv_extension spirv_extensions[3];
    struct vkd3d_shader_varying_map varying_map[12];
    struct vkd3d_shader_resource_binding *binding;
    struct vkd3d_shader_compile_option options[3];
    struct vkd3d_shader_parameter1 parameters[41];
    uint32_t offset, size;
    unsigned int i;
    char *messages;
    int ret;

    options[0].name = VKD3D_SHADER_COMPILE_OPTION_API_VERSION;
    options[0].value = VKD3D_SHADER_API_VERSION_CURRENT;
    options[1].name = VKD3D_SHADER_COMPILE_OPTION_FEATURE;
    options[1].value = shader_runner_caps_get_feature_flags(&runner->caps);
    options[2].name = VKD3D_SHADER_COMPILE_OPTION_TYPED_UAV;
    if (runner->caps.format_caps[DXGI_FORMAT_UNKNOWN] & FORMAT_CAP_UAV_LOAD)
        options[2].value = VKD3D_SHADER_COMPILE_OPTION_TYPED_UAV_READ_FORMAT_UNKNOWN;
    else
        options[2].value = VKD3D_SHADER_COMPILE_OPTION_TYPED_UAV_READ_FORMAT_R32;

    info.next = &spirv_info;
    info.source.code = ID3D10Blob_GetBufferPointer(runner->d3d_blobs[type]);
    info.source.size = ID3D10Blob_GetBufferSize(runner->d3d_blobs[type]);
    if (runner->r.shader_model < SHADER_MODEL_4_0)
        info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
    else if (runner->r.minimum_shader_model < SHADER_MODEL_6_0)
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_TPF;
    else
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_DXIL;
    info.target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY;
    info.log_level = VKD3D_SHADER_LOG_WARNING;
    info.option_count = ARRAY_SIZE(options);
    info.options = options;

    if (type == SHADER_TYPE_VS && info.source_type == VKD3D_SHADER_SOURCE_D3D_BYTECODE)
    {
        info.next = &varying_map_info;

        varying_map_info.next = &spirv_info;
        varying_map_info.varying_map = varying_map;
        vkd3d_shader_build_varying_map(&runner->signatures[SHADER_TYPE_VS].output,
                &runner->signatures[SHADER_TYPE_PS].input, &varying_map_info.varying_count, varying_map);
        ok(varying_map_info.varying_count <= ARRAY_SIZE(varying_map),
                "Got unexpected count %u.\n", varying_map_info.varying_count);
    }
    else if (type == SHADER_TYPE_PS && info.source_type == VKD3D_SHADER_SOURCE_D3D_BYTECODE)
    {
        info.next = &d3dbc_source_info;

        d3dbc_source_info.next = &spirv_info;

        for (i = 0; i < runner->r.resource_count; ++i)
        {
            const struct resource *resource = runner->r.resources[i];

            if (resource->desc.type != RESOURCE_TYPE_TEXTURE
                    || resource->desc.slot > ARRAY_SIZE(d3dbc_source_info.texture_dimensions))
                continue;

            if (resource->desc.dimension == RESOURCE_DIMENSION_3D)
                d3dbc_source_info.texture_dimensions[resource->desc.slot] = VKD3D_SHADER_RESOURCE_TEXTURE_3D;
            else if (resource->desc.dimension == RESOURCE_DIMENSION_CUBE)
                d3dbc_source_info.texture_dimensions[resource->desc.slot] = VKD3D_SHADER_RESOURCE_TEXTURE_CUBE;
            else
                d3dbc_source_info.texture_dimensions[resource->desc.slot] = VKD3D_SHADER_RESOURCE_TEXTURE_2D;
        }
    }

    spirv_info.next = &interface_info;
    if (runner->context.vk_api_version >= VK_API_VERSION_1_1)
        spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_1;
    else
        spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;
    spirv_info.extensions = spirv_extensions;
    spirv_info.extension_count = 0;

    if (runner->caps.shader_caps[SHADER_CAP_ROV])
        spirv_extensions[spirv_info.extension_count++] = VKD3D_SHADER_SPIRV_EXTENSION_EXT_FRAGMENT_SHADER_INTERLOCK;
    if (runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX])
        spirv_extensions[spirv_info.extension_count++] = VKD3D_SHADER_SPIRV_EXTENSION_EXT_VIEWPORT_INDEX_LAYER;
    if (runner->demote_to_helper_invocation)
        spirv_extensions[spirv_info.extension_count++] = VKD3D_SHADER_SPIRV_EXTENSION_EXT_DEMOTE_TO_HELPER_INVOCATION;

    for (i = 0, offset = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        struct vkd3d_shader_push_constant_buffer *p = &push_constants[i];
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        size = u->count * sizeof(*u->elements);
        p->register_space = 0;
        p->register_index = i;
        p->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        p->offset = offset;
        p->size = size;
        offset += size;
    }

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *mapping = &runner->r.descriptors[i];

        binding = &bindings[interface_info.binding_count++];
        binding->type = mapping->type;
        binding->register_space = mapping->register_space;
        binding->register_index = mapping->register_idx;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = is_mapping_buffer(runner, mapping) ?
                VKD3D_SHADER_BINDING_FLAG_BUFFER : VKD3D_SHADER_BINDING_FLAG_IMAGE;
        binding->binding.set = 0;
        binding->binding.binding = i;
        binding->binding.count = mapping->count;
    }

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        binding = &bindings[interface_info.binding_count++];
        binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_SAMPLER;
        binding->register_space = 0;
        binding->register_index = runner->r.samplers[i].slot;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = 0;
        binding->binding.set = 0;
        binding->binding.binding = runner->samplers[i].binding;
        binding->binding.count = 1;
    }

    interface_info.next = &parameter_info;
    interface_info.bindings = bindings;

    interface_info.push_constant_buffer_count = ARRAY_SIZE(push_constants);
    interface_info.push_constant_buffers = push_constants;

    parameters[0].name = VKD3D_SHADER_PARAMETER_NAME_RASTERIZER_SAMPLE_COUNT;
    parameters[0].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[0].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[0].u.immediate_constant.u.u32 = runner->r.sample_count;

    parameters[1].name = VKD3D_SHADER_PARAMETER_NAME_ALPHA_TEST_FUNC;
    parameters[1].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[1].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[1].u.immediate_constant.u.u32 = runner->r.alpha_test_func;

    parameters[2].name = VKD3D_SHADER_PARAMETER_NAME_ALPHA_TEST_REF;
    parameters[2].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[2].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
    parameters[2].u.immediate_constant.u.f32 = runner->r.alpha_test_ref;

    parameters[3].name = VKD3D_SHADER_PARAMETER_NAME_FLAT_INTERPOLATION;
    parameters[3].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[3].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[3].u.immediate_constant.u.u32 = runner->r.flat_shading;

    parameters[4].name = VKD3D_SHADER_PARAMETER_NAME_CLIP_PLANE_MASK;
    parameters[4].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[4].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[4].u.immediate_constant.u.u32 = runner->r.clip_plane_mask;

    for (i = 0; i < 8; ++i)
    {
        parameters[5 + i].name = VKD3D_SHADER_PARAMETER_NAME_CLIP_PLANE_0 + i;
        parameters[5 + i].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
        parameters[5 + i].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32_VEC4;
        memcpy(parameters[5 + i].u.immediate_constant.u.f32_vec4, &runner->r.clip_planes[i], 4 * sizeof(float));
    }

    parameters[13].name = VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE;
    parameters[13].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[13].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
    parameters[13].u.immediate_constant.u.f32 = runner->r.point_size;

    parameters[14].name = VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE_MIN;
    parameters[14].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[14].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
    parameters[14].u.immediate_constant.u.f32 = runner->r.point_size_min;

    parameters[15].name = VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE_MAX;
    parameters[15].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[15].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
    parameters[15].u.immediate_constant.u.f32 = runner->r.point_size_max;

    parameters[16].name = VKD3D_SHADER_PARAMETER_NAME_POINT_SPRITE;
    parameters[16].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[16].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[16].u.immediate_constant.u.u32 = runner->r.point_sprite;

    parameters[17].name = VKD3D_SHADER_PARAMETER_NAME_FOG_COLOUR;
    parameters[17].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[17].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32_VEC4;
    memcpy(parameters[17].u.immediate_constant.u.f32_vec4, &runner->r.fog_colour, sizeof(struct vec4));

    parameters[18].name = VKD3D_SHADER_PARAMETER_NAME_FOG_FRAGMENT_MODE;
    parameters[18].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[18].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[18].u.immediate_constant.u.u32 = get_fog_fragment_mode(runner->r.fog_mode);

    parameters[19].name = VKD3D_SHADER_PARAMETER_NAME_FOG_END;
    parameters[19].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[19].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;

    parameters[20].name = VKD3D_SHADER_PARAMETER_NAME_FOG_SCALE;
    parameters[20].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[20].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;

    switch (runner->r.fog_mode)
    {
        case FOG_MODE_NONE:
            parameters[19].u.immediate_constant.u.f32 = 0.0f;
            parameters[20].u.immediate_constant.u.f32 = -1.0f;
            break;

        case FOG_MODE_LINEAR:
            parameters[19].u.immediate_constant.u.f32 = runner->r.fog_end;
            parameters[20].u.immediate_constant.u.f32 = 1.0 / (runner->r.fog_end - runner->r.fog_start);
            break;

        case FOG_MODE_EXP:
            parameters[20].u.immediate_constant.u.f32 = runner->r.fog_density * LOG2_E;
            break;

        case FOG_MODE_EXP2:
            parameters[20].u.immediate_constant.u.f32 = runner->r.fog_density * SQRT_LOG2_E;
            break;

        case FOG_MODE_DISABLE:
            break;
    }

    parameters[21].name = VKD3D_SHADER_PARAMETER_NAME_FOG_SOURCE;
    parameters[21].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[21].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[21].u.immediate_constant.u.u32 = get_fog_source(&runner->r);

    for (i = 0; i < 6; ++i)
    {
        parameters[22 + i].name = VKD3D_SHADER_PARAMETER_NAME_BUMP_MATRIX_0 + i;
        parameters[22 + i].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
        parameters[22 + i].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32_VEC4;
        memcpy(parameters[22 + i].u.immediate_constant.u.f32_vec4, runner->r.bump[i].matrix, sizeof(struct vec4));

        parameters[28 + i].name = VKD3D_SHADER_PARAMETER_NAME_BUMP_LUMINANCE_SCALE_0 + i;
        parameters[28 + i].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
        parameters[28 + i].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
        parameters[28 + i].u.immediate_constant.u.f32 = runner->r.bump[i].luminance_scale;

        parameters[34 + i].name = VKD3D_SHADER_PARAMETER_NAME_BUMP_LUMINANCE_OFFSET_0 + i;
        parameters[34 + i].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
        parameters[34 + i].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32;
        parameters[34 + i].u.immediate_constant.u.f32 = runner->r.bump[i].luminance_offset;
    }

    parameters[40].name = VKD3D_SHADER_PARAMETER_NAME_PROJECTED_TEXTURE_MASK;
    parameters[40].type = VKD3D_SHADER_PARAMETER_TYPE_IMMEDIATE_CONSTANT;
    parameters[40].data_type = VKD3D_SHADER_PARAMETER_DATA_TYPE_UINT32;
    parameters[40].u.immediate_constant.u.u32 = runner->r.projected_texture_mask;

    parameter_info.parameter_count = ARRAY_SIZE(parameters);
    parameter_info.parameters = parameters;

    ret = vkd3d_shader_compile(&info, spirv, &messages);
    if (messages && vkd3d_test_state.debug_level)
        trace("%s\n", messages);
    vkd3d_shader_free_messages(messages);
    if (ret)
        return false;

    return true;
}

static bool create_shader_stage(struct vulkan_shader_runner *runner,
        VkPipelineShaderStageCreateInfo *stage_info, enum shader_type type, enum VkShaderStageFlagBits stage)
{
    VkShaderModuleCreateInfo module_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    const struct vulkan_test_context *context = &runner->context;
    struct vkd3d_shader_code spirv;

    if (!compile_d3d_code(runner, type, &spirv))
        return false;

    memset(stage_info, 0, sizeof(*stage_info));
    stage_info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info->stage = stage;
    stage_info->pName = "main";

    module_info.codeSize = spirv.size;
    module_info.pCode = spirv.code;

    VK_CALL(vkCreateShaderModule(context->device, &module_info, NULL, &stage_info->module));
    vkd3d_shader_free_shader_code(&spirv);
    return true;
}

static VkPrimitiveTopology vulkan_primitive_topology_from_d3d(D3D_PRIMITIVE_TOPOLOGY topology)
{
    switch (topology)
    {
        default:
            if (topology >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
                    && topology <= D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST)
                return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            fatal_error("Unhandled primitive topology %#x.\n", topology);
            /* fall through */
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
}

static VkPipelineLayout create_pipeline_layout(const struct vulkan_shader_runner *runner,
        VkDescriptorSetLayout set_layout)
{
    VkPipelineLayoutCreateInfo layout_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    const struct vulkan_test_context *context = &runner->context;
    VkPushConstantRange push_constant_range;
    VkPipelineLayout pipeline_layout;
    size_t size, i;

    layout_desc.setLayoutCount = 1;
    layout_desc.pSetLayouts = &set_layout;

    for (i = 0, size = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        size += u->count * sizeof(*u->elements);
    }

    if (size)
    {
        layout_desc.pushConstantRangeCount = 1;
        layout_desc.pPushConstantRanges = &push_constant_range;

        push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
        push_constant_range.offset = 0;
        push_constant_range.size = size;
    }

    VK_CALL(vkCreatePipelineLayout(context->device, &layout_desc, NULL, &pipeline_layout));

    return pipeline_layout;
}

static enum VkCompareOp vk_compare_op_from_d3d12(D3D12_COMPARISON_FUNC op)
{
    switch (op)
    {
        case D3D12_COMPARISON_FUNC_NEVER:
            return VK_COMPARE_OP_NEVER;
        case D3D12_COMPARISON_FUNC_LESS:
            return VK_COMPARE_OP_LESS;
        case D3D12_COMPARISON_FUNC_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case D3D12_COMPARISON_FUNC_LESS_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case D3D12_COMPARISON_FUNC_GREATER:
            return VK_COMPARE_OP_GREATER;
        case D3D12_COMPARISON_FUNC_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case D3D12_COMPARISON_FUNC_GREATER_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case D3D12_COMPARISON_FUNC_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            fatal_error("Unhandled compare op %#x.\n", op);
    }
}

static VkPipeline create_graphics_pipeline(struct vulkan_shader_runner *runner, VkRenderPass render_pass,
        VkPipelineLayout pipeline_layout, D3D_PRIMITIVE_TOPOLOGY primitive_topology)
{
    VkPipelineInputAssemblyStateCreateInfo ia_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rs_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineVertexInputStateCreateInfo input_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineColorBlendStateCreateInfo blend_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo ms_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo vp_desc = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo pipeline_desc = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkViewport viewports[ARRAY_SIZE(runner->r.viewports)] = {{.y = runner->rt_size.height,
            .width = runner->rt_size.width, .height = -(float)runner->rt_size.height, .maxDepth = 1}};
    VkRect2D scissor_rects[ARRAY_SIZE(runner->r.viewports)] = {{.extent = runner->rt_size}};
    VkPipelineColorBlendAttachmentState attachment_desc[MAX_RESOURCES] = {0};
    VkVertexInputBindingDescription input_bindings[MAX_INPUT_BINDINGS];
    const struct vulkan_test_context *context = &runner->context;
    VkPipelineTessellationStateCreateInfo tessellation_info;
    VkVertexInputAttributeDescription input_attributes[32];
    VkPipelineDepthStencilStateCreateInfo ds_desc = {0};
    unsigned int strides[MAX_INPUT_BINDINGS] = {0};
    VkPipelineShaderStageCreateInfo stage_desc[5];
    unsigned int stage_count = 0, viewport_count;
    VkDevice device = context->device;
    VkPipeline pipeline;
    unsigned int i;
    VkResult vr;
    int ret;

    ret = compile_hlsl_and_scan(runner, SHADER_TYPE_VS);
    ret &= compile_hlsl_and_scan(runner, SHADER_TYPE_PS);
    if (runner->r.shader_source[SHADER_TYPE_HS])
    {
        ret &= compile_hlsl_and_scan(runner, SHADER_TYPE_HS);
        ret &= compile_hlsl_and_scan(runner, SHADER_TYPE_DS);
    }
    if (runner->r.shader_source[SHADER_TYPE_GS])
        ret &= compile_hlsl_and_scan(runner, SHADER_TYPE_GS);

    if (!ret)
    {
        trace("Failed to compile HLSL shader(s).\n");
        free_d3d_blobs(runner);
        return VK_NULL_HANDLE;
    }

    memset(stage_desc, 0, sizeof(stage_desc));
    ret = create_shader_stage(runner, &stage_desc[stage_count++], SHADER_TYPE_VS, VK_SHADER_STAGE_VERTEX_BIT);
    ret &= create_shader_stage(runner, &stage_desc[stage_count++], SHADER_TYPE_PS, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (runner->r.shader_source[SHADER_TYPE_HS])
    {
        ret &= create_shader_stage(runner, &stage_desc[stage_count++],
                SHADER_TYPE_HS, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        ret &= create_shader_stage(runner, &stage_desc[stage_count++],
                SHADER_TYPE_DS, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    if (runner->r.shader_source[SHADER_TYPE_GS])
        ret &= create_shader_stage(runner, &stage_desc[stage_count++], SHADER_TYPE_GS, VK_SHADER_STAGE_GEOMETRY_BIT);

    if (!ret)
    {
        trace("Failed to compile SPIR-V shader(s).\n");
        for (i = 0; i < ARRAY_SIZE(stage_desc); ++i)
            VK_CALL(vkDestroyShaderModule(device, stage_desc[i].module, NULL));
        free_d3d_blobs(runner);
        return VK_NULL_HANDLE;
    }

    if (runner->r.input.element_count > ARRAY_SIZE(input_attributes))
        fatal_error("Input element count %zu is too high.\n", runner->r.input.element_count);

    for (i = 0; i < runner->r.input.element_count; ++i)
    {
        const struct input_element *element = &runner->r.input.elements[i];
        const struct vkd3d_shader_signature_element *signature_element;
        VkVertexInputAttributeDescription *attribute;
        unsigned int offset;

        if (element->slot >= ARRAY_SIZE(strides))
            fatal_error("Unsupported input slot %u.\n", element->slot);

        offset = strides[element->slot];
        strides[element->slot] += element->texel_size;

        if (!(signature_element = vkd3d_shader_find_signature_element(
                &runner->signatures[SHADER_TYPE_VS].input, element->name, element->index, 0)))
            continue;

        attribute = &input_attributes[input_desc.vertexAttributeDescriptionCount++];
        attribute->location = signature_element->register_index;
        attribute->binding = element->slot;
        attribute->format = vkd3d_get_vk_format(element->format);
        attribute->offset = offset;
    }

    input_desc.pVertexAttributeDescriptions = input_attributes;
    input_desc.pVertexBindingDescriptions = input_bindings;

    blend_desc.attachmentCount = 0;
    blend_desc.pAttachments = attachment_desc;

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        const struct vulkan_resource *resource = vulkan_resource(runner->r.resources[i]);

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UAV:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;

            case RESOURCE_TYPE_RENDER_TARGET:
                attachment_desc[blend_desc.attachmentCount++].colorWriteMask =
                        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
                break;

            case RESOURCE_TYPE_DEPTH_STENCIL:
                ds_desc.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                ds_desc.pNext = NULL;
                ds_desc.flags = 0;
                ds_desc.depthTestEnable = VK_TRUE;
                ds_desc.depthWriteEnable = VK_TRUE;
                ds_desc.depthCompareOp = vk_compare_op_from_d3d12(runner->r.depth_func);
                ds_desc.depthBoundsTestEnable = runner->r.depth_bounds;
                ds_desc.stencilTestEnable = VK_FALSE;
                ds_desc.minDepthBounds = runner->r.depth_min;
                ds_desc.maxDepthBounds = runner->r.depth_max;
                pipeline_desc.pDepthStencilState = &ds_desc;
                break;

            case RESOURCE_TYPE_VERTEX_BUFFER:
            {
                VkVertexInputBindingDescription *binding = &input_bindings[input_desc.vertexBindingDescriptionCount++];

                binding->binding = resource->r.desc.slot;
                binding->stride = strides[binding->binding];
                binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                break;
            }
        }
    }

    ia_desc.topology = vulkan_primitive_topology_from_d3d(primitive_topology);

    viewport_count = max(runner->r.viewport_count, 1);
    for (i = 0; i < runner->r.viewport_count; ++i)
    {
        viewports[i].x = runner->r.viewports[i].x;
        viewports[i].y = runner->r.viewports[i].y + runner->r.viewports[i].height;
        viewports[i].width = runner->r.viewports[i].width;
        viewports[i].height = -runner->r.viewports[i].height;
        viewports[i].maxDepth = 1.0f;
        scissor_rects[i].offset.x = 0;
        scissor_rects[i].offset.y = 0;
        scissor_rects[i].extent = runner->rt_size;
    }
    if (runner->r.shader_model < SHADER_MODEL_4_0)
    {
        for (i = 0; i < viewport_count; ++i)
        {
            viewports[i].x += 0.5f;
            viewports[i].y += 0.5f;
        }
    }

    vp_desc.viewportCount = viewport_count;
    vp_desc.pViewports = viewports;
    vp_desc.scissorCount = viewport_count;
    vp_desc.pScissors = scissor_rects;

    rs_desc.cullMode = VK_CULL_MODE_NONE;
    rs_desc.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs_desc.lineWidth = 1.0f;

    ms_desc.rasterizationSamples = runner->r.sample_count;
    ms_desc.pSampleMask = &runner->r.sample_mask;

    pipeline_desc.stageCount = stage_count;
    pipeline_desc.pStages = stage_desc;
    pipeline_desc.pVertexInputState = &input_desc;
    pipeline_desc.pInputAssemblyState = &ia_desc;
    pipeline_desc.pViewportState = &vp_desc;
    pipeline_desc.pRasterizationState = &rs_desc;
    pipeline_desc.pMultisampleState = &ms_desc;
    pipeline_desc.pColorBlendState = &blend_desc;
    pipeline_desc.layout = pipeline_layout;
    pipeline_desc.renderPass = render_pass;
    pipeline_desc.subpass = 0;

    if (runner->r.shader_source[SHADER_TYPE_HS])
    {
        tessellation_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessellation_info.pNext = NULL;
        tessellation_info.flags = 0;
        tessellation_info.patchControlPoints
                = max(primitive_topology - D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + 1, 1);
        pipeline_desc.pTessellationState = &tessellation_info;
    }

    vr = VK_CALL(vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipeline_desc, NULL, &pipeline));
    todo_if(runner->r.is_todo) bug_if(runner->r.is_bug)
    ok(vr == VK_SUCCESS, "Failed to create graphics pipeline, vr %d.\n", vr);

    for (i = 0; i < ARRAY_SIZE(stage_desc); ++i)
        VK_CALL(vkDestroyShaderModule(device, stage_desc[i].module, NULL));
    free_d3d_blobs(runner);

    return pipeline;
}

static VkPipeline create_compute_pipeline(struct vulkan_shader_runner *runner, VkPipelineLayout pipeline_layout)
{
    VkComputePipelineCreateInfo pipeline_desc = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    const struct vulkan_test_context *context = &runner->context;
    VkPipeline pipeline;
    VkResult vr;
    bool ret;

    ret = compile_hlsl_and_scan(runner, SHADER_TYPE_CS);
    if (!ret)
    {
        trace("Failed to compile HLSL shader.\n");
        return VK_NULL_HANDLE;
    }

    ret = create_shader_stage(runner, &pipeline_desc.stage, SHADER_TYPE_CS, VK_SHADER_STAGE_COMPUTE_BIT);
    if (!ret)
    {
        trace("Failed to compile SPIR-V shader.\n");
        free_d3d_blobs(runner);
        return VK_NULL_HANDLE;
    }

    pipeline_desc.layout = pipeline_layout;

    vr = VK_CALL(vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &pipeline_desc, NULL, &pipeline));
    todo_if(runner->r.is_todo) bug_if(runner->r.is_bug)
    ok(vr == VK_SUCCESS, "Failed to create compute pipeline, vr %d.\n", vr);

    VK_CALL(vkDestroyShaderModule(context->device, pipeline_desc.stage.module, NULL));
    free_d3d_blobs(runner);

    return pipeline;
}

static VkSamplerAddressMode vk_address_mode_from_d3d12(D3D12_TEXTURE_ADDRESS_MODE mode)
{
    switch (mode)
    {
        case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            fatal_error("Unhandled sampler address mode %#x.\n", mode);
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static VkSamplerReductionModeEXT vk_reduction_mode_from_d3d12(D3D12_FILTER filter)
{
    D3D12_FILTER_REDUCTION_TYPE r = D3D12_DECODE_FILTER_REDUCTION(filter);

    switch (r)
    {
        case D3D12_FILTER_REDUCTION_TYPE_STANDARD:
        case D3D12_FILTER_REDUCTION_TYPE_COMPARISON:
            return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
        case D3D12_FILTER_REDUCTION_TYPE_MINIMUM:
            return VK_SAMPLER_REDUCTION_MODE_MIN;
        case D3D12_FILTER_REDUCTION_TYPE_MAXIMUM:
            return VK_SAMPLER_REDUCTION_MODE_MAX;
        default:
            fatal_error("Unhandled reduction type %#x.\n", r);
    }
}

static VkDescriptorSetLayout create_descriptor_set_layout(struct vulkan_shader_runner *runner)
{
    VkDescriptorSetLayoutCreateInfo set_desc = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkDescriptorSetLayoutBinding bindings[MAX_RESOURCES + MAX_SAMPLERS];
    const struct vulkan_test_context *context = &runner->context;
    VkDescriptorSetLayoutBinding *binding;
    VkDescriptorSetLayout set_layout;
    uint32_t binding_index = 0;
    size_t i;

    if (runner->r.resource_count > ARRAY_SIZE(bindings))
        fatal_error("Resource count %zu is too high.\n", runner->r.resource_count);

    set_desc.pBindings = bindings;

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *mapping = &runner->r.descriptors[i];

        binding = &bindings[set_desc.bindingCount++];
        binding->binding = binding_index++;
        binding->descriptorCount = mapping->count;
        binding->stageFlags = VK_SHADER_STAGE_ALL;
        binding->pImmutableSamplers = NULL;

        switch (mapping->type)
        {
            case VKD3D_SHADER_DESCRIPTOR_TYPE_SRV:
                binding->descriptorType = is_mapping_buffer(runner, mapping) ?
                        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;

            case VKD3D_SHADER_DESCRIPTOR_TYPE_UAV:
                binding->descriptorType = is_mapping_buffer(runner, mapping) ?
                        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                break;

            default:
                fatal_error("Unexpected descriptor type %#x.\n", mapping->type);
        }
    }

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        VkSamplerCreateInfo sampler_desc = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        struct vulkan_sampler *vulkan_sampler = &runner->samplers[i];
        const struct sampler *sampler = &runner->r.samplers[i];
        VkSamplerReductionModeCreateInfoEXT reduction_desc;

        reduction_desc.reductionMode = vk_reduction_mode_from_d3d12(sampler->filter);
        if (reduction_desc.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
        {
            sampler_desc.pNext = &reduction_desc;
            reduction_desc.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
            reduction_desc.pNext = NULL;
        }

        sampler_desc.magFilter = (sampler->filter & 0x4) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_desc.minFilter = (sampler->filter & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler_desc.mipmapMode = (sampler->filter & 0x1) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_desc.addressModeU = vk_address_mode_from_d3d12(sampler->u_address);
        sampler_desc.addressModeV = vk_address_mode_from_d3d12(sampler->v_address);
        sampler_desc.addressModeW = vk_address_mode_from_d3d12(sampler->w_address);
        sampler_desc.compareEnable = !!sampler->func;
        sampler_desc.compareOp = sampler->func ? vk_compare_op_from_d3d12(sampler->func) : 0;
        sampler_desc.maxLod = FLT_MAX;

        VK_CALL(vkCreateSampler(context->device, &sampler_desc, NULL, &vulkan_sampler->vk_sampler));
        vulkan_sampler->binding = binding_index++;

        binding = &bindings[set_desc.bindingCount++];

        binding->binding = vulkan_sampler->binding;
        binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        binding->descriptorCount = 1;
        binding->stageFlags = VK_SHADER_STAGE_ALL;
        binding->pImmutableSamplers = &vulkan_sampler->vk_sampler;
    }

    VK_CALL(vkCreateDescriptorSetLayout(context->device, &set_desc, NULL, &set_layout));

    return set_layout;
}

static void bind_resources(struct vulkan_shader_runner *runner, VkPipelineBindPoint bind_point,
        VkDescriptorSetLayout set_layout, VkPipelineLayout pipeline_layout)
{
    VkDescriptorSetAllocateInfo set_desc = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    const struct vulkan_test_context *context = &runner->context;
    VkCommandBuffer cmd_buffer = context->cmd_buffer;
    VkDescriptorSet descriptor_set;
    uint32_t offset, size;
    unsigned int i, j;

    set_desc.descriptorPool = context->descriptor_pool;
    set_desc.descriptorSetCount = 1;
    set_desc.pSetLayouts = &set_layout;

    VK_CALL(vkAllocateDescriptorSets(context->device, &set_desc, &descriptor_set));

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *mapping = &runner->r.descriptors[i];

        for (j = 0; j < mapping->count; ++j)
        {
            VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            const struct vulkan_resource *resource = find_resource(runner, mapping, j);
            VkDescriptorImageInfo image_info = { 0 };

            write.dstSet = descriptor_set;
            write.dstBinding = i;
            write.dstArrayElement = j;
            write.descriptorCount = 1;

            switch (mapping->type)
            {
                case VKD3D_SHADER_DESCRIPTOR_TYPE_SRV:
                    if (is_mapping_buffer(runner, mapping))
                    {
                        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                        write.pTexelBufferView = &resource->buffer_view;
                    }
                    else
                    {
                        image_info.imageView = resource->image_view;
                        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        write.pImageInfo = &image_info;
                    }
                    break;

                case VKD3D_SHADER_DESCRIPTOR_TYPE_UAV:
                    if (is_mapping_buffer(runner, mapping))
                    {
                        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                        write.pTexelBufferView = &resource->buffer_view;
                    }
                    else
                    {
                        image_info.imageView = resource->image_view;
                        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        write.pImageInfo = &image_info;
                    }
                    break;

                default:
                    fatal_error("Invalid descriptor type %#x.\n", mapping->type);
            }

            VK_CALL(vkUpdateDescriptorSets(context->device, 1, &write, 0, NULL));
        }
    }

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        const struct vulkan_resource *resource = vulkan_resource(runner->r.resources[i]);
        static const VkDeviceSize zero_offset;

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_VERTEX_BUFFER:
                if (bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS)
                    VK_CALL(vkCmdBindVertexBuffers(cmd_buffer, resource->r.desc.slot,
                            1, &resource->buffer, &zero_offset));
                break;

            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UAV:
            case RESOURCE_TYPE_RENDER_TARGET:
            case RESOURCE_TYPE_DEPTH_STENCIL:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }

    VK_CALL(vkCmdBindDescriptorSets(cmd_buffer, bind_point, pipeline_layout, 0, 1, &descriptor_set, 0, NULL));

    for (i = 0, offset = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        size = u->count * sizeof(*u->elements);
        VK_CALL(vkCmdPushConstants(cmd_buffer, pipeline_layout, VK_SHADER_STAGE_ALL, offset, size, u->elements));
        offset += size;
    }

    /* The descriptor set will be freed by resetting the descriptor pool. */
}

static void create_render_pass_and_framebuffer(struct vulkan_shader_runner *runner,
        VkRenderPass *render_pass, VkFramebuffer *fb)
{
    VkRenderPassCreateInfo render_pass_desc = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    VkFramebufferCreateInfo fb_desc = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    VkAttachmentReference ds_ref = {0}, color_refs[MAX_RESOURCES] = {0};
    VkAttachmentDescription attachment_descs[MAX_RESOURCES] = {0};
    const struct vulkan_test_context *context = &runner->context;
    unsigned int i, color_ref_count = 0, view_count = 0;
    VkSubpassDescription subpass_desc = {0};
    VkImageView views[MAX_RESOURCES];
    unsigned int layer_count = ~0u;
    VkImageLayout layout;
    bool is_ds;

    runner->rt_size.width = ~0u;
    runner->rt_size.height = ~0u;
    for (i = 0; i < runner->r.resource_count; ++i)
    {
        const struct vulkan_resource *resource = vulkan_resource(runner->r.resources[i]);
        VkAttachmentDescription *attachment_desc = &attachment_descs[view_count];
        VkAttachmentReference *color_ref = &color_refs[color_ref_count];

        if (resource->r.desc.type != RESOURCE_TYPE_RENDER_TARGET && resource->r.desc.type != RESOURCE_TYPE_DEPTH_STENCIL)
            continue;

        is_ds = resource->r.desc.type == RESOURCE_TYPE_DEPTH_STENCIL;
        layout = is_ds ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachment_desc->format = vkd3d_get_vk_format(resource->r.desc.format);
        attachment_desc->samples = max(resource->r.desc.sample_count, 1);
        attachment_desc->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment_desc->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_desc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_desc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment_desc->initialLayout = layout;
        attachment_desc->finalLayout = layout;

        if (is_ds)
        {
            ds_ref.attachment = view_count;
            ds_ref.layout = layout;
            subpass_desc.pDepthStencilAttachment = &ds_ref;
        }
        else
        {
            color_ref->attachment = view_count;
            color_ref->layout = layout;
            ++color_ref_count;
        }

        if (resource->r.desc.width < runner->rt_size.width)
            runner->rt_size.width = resource->r.desc.width;
        if (resource->r.desc.height < runner->rt_size.height)
            runner->rt_size.height = resource->r.desc.height;
        if (resource->r.desc.layer_count < layer_count)
            layer_count = resource->r.desc.layer_count;

        views[view_count++] = resource->image_view;
    }

    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = color_ref_count;
    subpass_desc.pColorAttachments = color_refs;

    render_pass_desc.attachmentCount = view_count;
    render_pass_desc.pAttachments = attachment_descs;
    render_pass_desc.subpassCount = 1;
    render_pass_desc.pSubpasses = &subpass_desc;

    VK_CALL(vkCreateRenderPass(context->device, &render_pass_desc, NULL, render_pass));

    fb_desc.renderPass = *render_pass;
    fb_desc.attachmentCount = view_count;
    fb_desc.pAttachments = views;
    fb_desc.width = runner->rt_size.width;
    fb_desc.height = runner->rt_size.height;
    fb_desc.layers = layer_count;

    VK_CALL(vkCreateFramebuffer(context->device, &fb_desc, NULL, fb));
}

static bool vulkan_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    VkCommandBuffer cmd_buffer = context->cmd_buffer;
    VkDevice device = context->device;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    bool ret = false;
    unsigned int i;

    /* Create this before compiling shaders, it will assign resource bindings. */
    set_layout = create_descriptor_set_layout(runner);

    pipeline_layout = create_pipeline_layout(runner, set_layout);
    if (!(pipeline = create_compute_pipeline(runner, pipeline_layout)))
        goto out;

    begin_command_buffer(context);

    VK_CALL(vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline));

    bind_resources(runner, VK_PIPELINE_BIND_POINT_COMPUTE, set_layout, pipeline_layout);

    VK_CALL(vkCmdDispatch(cmd_buffer, x, y, z));

    end_command_buffer(context);

    VK_CALL(vkDestroyPipeline(device, pipeline, NULL));
    VK_CALL(vkResetDescriptorPool(device, context->descriptor_pool, 0));

    ret = true;
out:
    for (i = 0; i < runner->r.sampler_count; ++i)
        VK_CALL(vkDestroySampler(device, runner->samplers[i].vk_sampler, NULL));

    VK_CALL(vkDestroyPipelineLayout(device, pipeline_layout, NULL));
    VK_CALL(vkDestroyDescriptorSetLayout(device, set_layout, NULL));

    return ret;
}

static void vulkan_runner_clear(struct shader_runner *r, struct resource *res, const struct vec4 *clear_value)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    struct vulkan_resource *resource = vulkan_resource(res);

    size_t width = resource->r.desc.width, height = resource->r.desc.height;
    const struct vulkan_test_context *context = &runner->context;
    VkSubpassDescription sub_pass_desc = {0};
    VkAttachmentDescription attachment_desc;
    VkRenderPassCreateInfo pass_desc = {0};
    VkAttachmentReference attachment_ref;
    VkDevice device = context->device;
    VkRenderPassBeginInfo begin_desc;
    VkFramebufferCreateInfo fb_desc;
    VkClearValue vk_clear_value;
    VkRenderPass render_pass;
    VkFramebuffer fb;

    attachment_desc.flags = 0;
    attachment_desc.format = vkd3d_get_vk_format(resource->r.desc.format);
    attachment_desc.samples = max(resource->r.desc.sample_count, 1);
    attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    /* TODO: formats with a stencil component would a clear op here. */
    attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    sub_pass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            attachment_desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            sub_pass_desc.colorAttachmentCount = 1;
            sub_pass_desc.pColorAttachments = &attachment_ref;
            memcpy(vk_clear_value.color.float32, clear_value, sizeof(vk_clear_value.color.float32));
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            attachment_desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            sub_pass_desc.pDepthStencilAttachment = &attachment_ref;
            vk_clear_value.depthStencil.depth = clear_value->x;
            vk_clear_value.depthStencil.stencil = 0;
            break;

        default:
            fatal_error("Clears are not implemented for resource type %u.\n", resource->r.desc.type);
    }

    attachment_desc.finalLayout = attachment_desc.initialLayout;

    attachment_ref.attachment = 0;
    attachment_ref.layout = attachment_desc.initialLayout;

    begin_command_buffer(context);

    pass_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_desc.attachmentCount = 1;
    pass_desc.pAttachments = &attachment_desc;
    pass_desc.subpassCount = 1;
    pass_desc.pSubpasses = &sub_pass_desc;
    VK_CALL(vkCreateRenderPass(device, &pass_desc, NULL, &render_pass));

    fb_desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_desc.pNext = NULL;
    fb_desc.flags = 0;
    fb_desc.renderPass = render_pass;
    fb_desc.attachmentCount = 1;
    fb_desc.pAttachments = &resource->image_view;
    fb_desc.width = width;
    fb_desc.height = height;
    fb_desc.layers = resource->r.desc.layer_count;
    VK_CALL(vkCreateFramebuffer(device, &fb_desc, NULL, &fb));

    begin_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_desc.pNext = NULL;
    begin_desc.renderPass = render_pass;
    begin_desc.framebuffer = fb;
    begin_desc.clearValueCount = 1;
    begin_desc.pClearValues = &vk_clear_value;
    begin_desc.renderArea.offset.x = 0;
    begin_desc.renderArea.offset.y = 0;
    begin_desc.renderArea.extent.width = width;
    begin_desc.renderArea.extent.height = height;
    VK_CALL(vkCmdBeginRenderPass(context->cmd_buffer, &begin_desc, VK_SUBPASS_CONTENTS_INLINE));
    VK_CALL(vkCmdEndRenderPass(context->cmd_buffer));

    end_command_buffer(context);

    VK_CALL(vkDestroyRenderPass(device, render_pass, NULL));
    VK_CALL(vkDestroyFramebuffer(device, fb, NULL));
}

static bool vulkan_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY primitive_topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    VkRenderPassBeginInfo pass_begin_desc = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    VkCommandBuffer cmd_buffer = context->cmd_buffer;
    VkDevice device = context->device;
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;
    VkPipeline pipeline;
    VkFramebuffer fb;
    bool ret = false;
    unsigned int i;

    create_render_pass_and_framebuffer(runner, &render_pass, &fb);

    /* Create this before compiling shaders, it will assign resource bindings. */
    set_layout = create_descriptor_set_layout(runner);

    pipeline_layout = create_pipeline_layout(runner, set_layout);
    if (!(pipeline = create_graphics_pipeline(runner, render_pass, pipeline_layout, primitive_topology)))
        goto out;

    begin_command_buffer(context);

    pass_begin_desc.renderPass = render_pass;
    pass_begin_desc.framebuffer = fb;
    pass_begin_desc.renderArea.extent = runner->rt_size;

    VK_CALL(vkCmdBeginRenderPass(cmd_buffer, &pass_begin_desc, VK_SUBPASS_CONTENTS_INLINE));

    VK_CALL(vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline));

    bind_resources(runner, VK_PIPELINE_BIND_POINT_GRAPHICS, set_layout, pipeline_layout);

    VK_CALL(vkCmdDraw(cmd_buffer, vertex_count, instance_count, first_vertex, first_instance));

    VK_CALL(vkCmdEndRenderPass(cmd_buffer));
    end_command_buffer(context);

    VK_CALL(vkDestroyPipeline(device, pipeline, NULL));
    VK_CALL(vkResetDescriptorPool(device, context->descriptor_pool, 0));

    ret = true;
out:
    for (i = 0; i < runner->r.sampler_count; ++i)
        VK_CALL(vkDestroySampler(device, runner->samplers[i].vk_sampler, NULL));

    VK_CALL(vkDestroyPipelineLayout(device, pipeline_layout, NULL));
    VK_CALL(vkDestroyDescriptorSetLayout(device, set_layout, NULL));
    VK_CALL(vkDestroyRenderPass(device, render_pass, NULL));
    VK_CALL(vkDestroyFramebuffer(device, fb, NULL));

    return ret;
}

static VkImageLayout resource_get_layout(struct resource *r)
{
    if (r->desc.type == RESOURCE_TYPE_RENDER_TARGET)
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (r->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (r->desc.type == RESOURCE_TYPE_TEXTURE)
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return VK_IMAGE_LAYOUT_GENERAL;
}

static bool vulkan_runner_copy(struct shader_runner *r, struct resource *src, struct resource *dst)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    struct vulkan_resource *s = vulkan_resource(src);
    struct vulkan_resource *d = vulkan_resource(dst);
    VkImageLayout src_layout, dst_layout;
    VkImageCopy vk_image_copy;
    unsigned int l;

    if (src->desc.dimension == RESOURCE_DIMENSION_BUFFER || src->desc.layer_count > 1)
        return false;

    if (src->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;

    src_layout = resource_get_layout(src);
    dst_layout = resource_get_layout(dst);

    begin_command_buffer(context);
    transition_image_layout(context, s->image, aspect_mask, 0, 1, src_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image_layout(context, d->image, aspect_mask, 0, 1,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_image_copy.srcSubresource = (VkImageSubresourceLayers){.aspectMask = aspect_mask, .layerCount = 1};
    vk_image_copy.srcOffset = (VkOffset3D){.x = 0, .y = 0, .z = 0};
    vk_image_copy.dstSubresource = vk_image_copy.srcSubresource;
    vk_image_copy.dstOffset = vk_image_copy.srcOffset;

    for (l = 0; l < src->desc.level_count; ++l)
    {
        vk_image_copy.srcSubresource.mipLevel = l;
        vk_image_copy.dstSubresource.mipLevel = l;
        vk_image_copy.extent.width = get_level_dimension(src->desc.width, l);
        vk_image_copy.extent.height = get_level_dimension(src->desc.height, l);
        vk_image_copy.extent.depth = get_level_dimension(src->desc.depth, l);

        VK_CALL(vkCmdCopyImage(context->cmd_buffer, s->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                d->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vk_image_copy));
    }

    transition_image_layout(context, d->image, aspect_mask, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_layout);
    transition_image_layout(context, s->image, aspect_mask, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_layout);
    end_command_buffer(context);

    return true;
}

struct vulkan_resource_readback
{
    struct resource_readback rb;
    VkDeviceMemory memory;
    VkBuffer buffer;
};

static struct resource_readback *vulkan_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    struct vulkan_resource_readback *rb = malloc(sizeof(*rb));
    struct vulkan_resource *resource = vulkan_resource(res);
    VkDevice device = context->device;
    VkImageAspectFlags aspect_mask;
    VkBufferImageCopy region = {0};
    unsigned int layer, level;
    VkImageLayout layout;
    size_t slice_pitch;

    rb->rb.width = resource->r.desc.width;
    rb->rb.height = resource->r.desc.height;
    rb->rb.depth = resource->r.desc.depth;

    rb->rb.row_pitch = rb->rb.width * resource->r.desc.texel_size;
    slice_pitch = rb->rb.row_pitch * rb->rb.height;

    rb->buffer = create_vulkan_buffer(context, slice_pitch * rb->rb.depth,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &rb->memory);

    if (resource->r.desc.type == RESOURCE_TYPE_UAV && resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
    {
        void *data;

        VK_CALL(vkMapMemory(device, resource->memory, 0, VK_WHOLE_SIZE, 0, &data));
        VK_CALL(vkMapMemory(device, rb->memory, 0, VK_WHOLE_SIZE, 0, &rb->rb.data));
        memcpy(rb->rb.data, data, rb->rb.row_pitch * rb->rb.height);
        VK_CALL(vkUnmapMemory(device, resource->memory));
    }
    else
    {
        struct resource_desc resolved_desc = resource->r.desc;
        VkImage resolved_image = VK_NULL_HANDLE;
        VkDeviceMemory resolved_memory;

        level = sub_resource_idx % resource->r.desc.level_count;
        layer = sub_resource_idx / resource->r.desc.level_count;
        aspect_mask = (resource->r.desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
                ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        layout = resource_get_layout(res);

        begin_command_buffer(context);

        transition_image_layout(context, resource->image, aspect_mask,
                layer, 1, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        region.imageSubresource.aspectMask = aspect_mask;
        region.imageSubresource.mipLevel = level;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = resource->r.desc.width;
        region.imageExtent.height = resource->r.desc.height;
        region.imageExtent.depth = resource->r.desc.depth;

        if (resource->r.desc.sample_count > 1)
        {
            VkImageResolve resolve_region = {{0}};

            resolve_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolve_region.srcSubresource.layerCount = 1;
            resolve_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            resolve_region.dstSubresource.layerCount = 1;
            resolve_region.extent.width = resource->r.desc.width;
            resolve_region.extent.height = resource->r.desc.height;
            resolve_region.extent.depth = 1;

            resolved_desc.sample_count = 1;
            resolved_image = create_vulkan_image(context, VK_IMAGE_TYPE_2D,
                    resolved_desc.width, resolved_desc.height, resolved_desc.depth,
                    resolved_desc.level_count, resolved_desc.layer_count, resolved_desc.sample_count,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    vkd3d_get_vk_format(resource->r.desc.format), 0, &resolved_memory);
            transition_image_layout(context, resolved_image, VK_IMAGE_ASPECT_COLOR_BIT, layer,
                    1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VK_CALL(vkCmdResolveImage(context->cmd_buffer, resource->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    resolved_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolve_region));
            transition_image_layout(context, resolved_image, VK_IMAGE_ASPECT_COLOR_BIT, layer,
                    1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VK_CALL(vkCmdCopyImageToBuffer(context->cmd_buffer, resolved_image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb->buffer, 1, &region));
        }
        else
        {
            VK_CALL(vkCmdCopyImageToBuffer(context->cmd_buffer, resource->image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb->buffer, 1, &region));
        }

        transition_image_layout(context, resource->image, aspect_mask,
                layer, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);

        end_command_buffer(context);

        if (resource->r.desc.sample_count > 1)
        {
            VK_CALL(vkFreeMemory(device, resolved_memory, NULL));
            VK_CALL(vkDestroyImage(device, resolved_image, NULL));
        }

        VK_CALL(vkMapMemory(device, rb->memory, 0, VK_WHOLE_SIZE, 0, &rb->rb.data));
    }

    return &rb->rb;
}

static void vulkan_runner_release_readback(struct shader_runner *r, struct resource_readback *rb)
{
    struct vulkan_resource_readback *vulkan_rb = CONTAINING_RECORD(rb, struct vulkan_resource_readback, rb);
    struct vulkan_shader_runner *runner = vulkan_shader_runner(r);
    const struct vulkan_test_context *context = &runner->context;
    VkDevice device = context->device;

    VK_CALL(vkUnmapMemory(device, vulkan_rb->memory));

    VK_CALL(vkFreeMemory(device, vulkan_rb->memory, NULL));
    VK_CALL(vkDestroyBuffer(device, vulkan_rb->buffer, NULL));
    free(vulkan_rb);
}

static const struct shader_runner_ops vulkan_runner_ops =
{
    .create_resource = vulkan_runner_create_resource,
    .destroy_resource = vulkan_runner_destroy_resource,
    .dispatch = vulkan_runner_dispatch,
    .clear = vulkan_runner_clear,
    .draw = vulkan_runner_draw,
    .copy = vulkan_runner_copy,
    .get_resource_readback = vulkan_runner_get_resource_readback,
    .release_readback = vulkan_runner_release_readback,
};

static bool is_option_separator(char c)
{
    return c == ',' || c == ';' || c == '\0';
}

static bool list_has_member(const char *string, const char *member)
{
    char prev_char, next_char;
    const char *p;

    p = string;
    while (p)
    {
        if ((p = strstr(p, member)))
        {
            prev_char = p > string ? p[-1] : 0;
            p += strlen(member);
            next_char = *p;

            if (is_option_separator(prev_char) && is_option_separator(next_char))
                return true;
        }
    }

    return false;
}

static bool check_device_extensions(struct vulkan_shader_runner *runner,
        struct vulkan_extension_list *enabled_extensions)
{
    const struct vulkan_test_context *context = &runner->context;
    VkPhysicalDevice phys_device = context->phys_device;
    VkExtensionProperties *extensions;
    const char *disabled_extensions;
    uint32_t i, count;

    static const struct
    {
        const char *name;
        bool required;
    }
    device_extensions[] =
    {
        {VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME},
        {VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME},
        {VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME},
        {VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME},
        {VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME},
        {VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, true},
        {VK_KHR_MAINTENANCE1_EXTENSION_NAME, true},
        {VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME},
    };

    enabled_extensions->names = calloc(ARRAY_SIZE(device_extensions), sizeof(*enabled_extensions->names));
    enabled_extensions->count = 0;

    VK_CALL(vkEnumerateDeviceExtensionProperties(phys_device, NULL, &count, NULL));
    extensions = calloc(count, sizeof(*extensions));
    VK_CALL(vkEnumerateDeviceExtensionProperties(phys_device, NULL, &count, extensions));

    disabled_extensions = getenv("VKD3D_DISABLE_EXTENSIONS");

    for (i = 0; i < ARRAY_SIZE(device_extensions); ++i)
    {
        const char *name = device_extensions[i].name;

        if (list_has_member(disabled_extensions, name))
            continue;

        if (vk_extension_properties_contain(extensions, count, name))
        {
            enabled_extensions->names[enabled_extensions->count++] = name;
            if (!strcmp(name, VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME))
                runner->caps.shader_caps[SHADER_CAP_ROV] = true;
            if (!strcmp(name, VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME))
                runner->caps.shader_caps[SHADER_CAP_MIN_LOD_CLAMP] = true;
            if (!strcmp(name, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME))
                runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX] = true;
            if (!strcmp(name, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME))
                runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX] = true;
            if (!strcmp(name, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME))
                runner->demote_to_helper_invocation = true;
            if (!strcmp(name, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
                runner->driver_properties = true;
            continue;
        }

        if (device_extensions[i].required)
        {
            skip("The selected Vulkan device does not support %s.\n", name);
            free(enabled_extensions->names);
            free(extensions);
            return false;
        }
    }

    free(extensions);
    return true;
}

static void get_physical_device_info(struct vulkan_shader_runner *runner, struct physical_device_info *info)
{
    const struct vulkan_test_context *context = &runner->context;
    memset(info, 0, sizeof(*info));

    info->features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    if (runner->caps.shader_caps[SHADER_CAP_ROV])
    {
        info->features2.pNext = &info->interlock_features;
        info->interlock_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
    }

    if (runner->demote_to_helper_invocation)
    {
        void *list = info->features2.pNext;

        info->features2.pNext = &info->demote_to_helper_invocation_features;
        info->demote_to_helper_invocation_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
        info->demote_to_helper_invocation_features.pNext = list;
    }

    if (context->vkGetPhysicalDeviceFeatures2KHR)
        VK_CALL(vkGetPhysicalDeviceFeatures2KHR(context->phys_device, &info->features2));
    else
        VK_CALL(vkGetPhysicalDeviceFeatures(context->phys_device, &info->features2.features));

    info->properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    if (runner->driver_properties)
    {
        void *list = info->properties2.pNext;

        info->properties2.pNext = &info->driver_properties;
        info->driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
        info->driver_properties.pNext = list;
    }

    if (runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX])
    {
        info->filter_minmax_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT;
        info->filter_minmax_properties.pNext = info->properties2.pNext;
        info->properties2.pNext = &info->filter_minmax_properties;
    }

    if (context->vk_api_version >= VK_API_VERSION_1_1)
    {
        info->subgroup_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        info->subgroup_properties.pNext = info->properties2.pNext;
        info->properties2.pNext = &info->subgroup_properties;
    }

    if (context->vkGetPhysicalDeviceFeatures2KHR)
        VK_CALL(vkGetPhysicalDeviceProperties2KHR(context->phys_device, &info->properties2));
    else
        VK_CALL(vkGetPhysicalDeviceProperties(context->phys_device, &info->properties2.properties));

    if (runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX]
            && (!info->filter_minmax_properties.filterMinmaxSingleComponentFormats
            || !info->filter_minmax_properties.filterMinmaxImageComponentMapping))
        runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX] = false;
}

static uint32_t get_format_support(const struct vulkan_test_context *context,
        const struct physical_device_info *device_info, enum DXGI_FORMAT format)
{
    VkFormatProperties properties;
    uint32_t ret = 0;

    VK_CALL(vkGetPhysicalDeviceFormatProperties(context->phys_device, vkd3d_get_vk_format(format), &properties));
    if (((properties.linearTilingFeatures | properties.optimalTilingFeatures) & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
            && ((format == DXGI_FORMAT_R32_FLOAT || format == DXGI_FORMAT_R32_UINT || format == DXGI_FORMAT_R32_SINT)
            || device_info->features2.features.shaderStorageImageReadWithoutFormat))
        ret |= FORMAT_CAP_UAV_LOAD;

    return ret;
}

static bool init_vulkan_runner(struct vulkan_shader_runner *runner)
{
    VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT demote_to_helper_invocation_features;
    VkDeviceQueueCreateInfo queue_desc = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    VkDeviceCreateInfo device_desc = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlock_features;
    const VkPhysicalDeviceSubgroupProperties *subgroup_properties;
    struct vulkan_test_context *context = &runner->context;
    struct vulkan_extension_list enabled_extensions;
    const VkPhysicalDeviceFeatures *ret_features;
    static const float queue_priority = 1.0f;
    struct physical_device_info device_info;
    VkPhysicalDeviceFeatures features;
    VkFormatProperties format_props;
    uint32_t graphics_index;
    bool b;

    static const char *instance_extensions[] =
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };
    static const enum DXGI_FORMAT formats[] =
    {
        DXGI_FORMAT_R32_FLOAT,
        DXGI_FORMAT_R32_UINT,
        DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_UINT,
        DXGI_FORMAT_R32G32B32A32_SINT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R16G16B16A16_UINT,
        DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R8G8B8A8_SINT,
        DXGI_FORMAT_R16_FLOAT,
        DXGI_FORMAT_R16_UINT,
        DXGI_FORMAT_R16_SINT,
        DXGI_FORMAT_R8_UNORM,
        DXGI_FORMAT_R8_UINT,
        DXGI_FORMAT_R8_SINT,
    };
    static const VkSubgroupFeatureFlags required_subgroup_features = VK_SUBGROUP_FEATURE_ARITHMETIC_BIT
            | VK_SUBGROUP_FEATURE_BASIC_BIT
            | VK_SUBGROUP_FEATURE_BALLOT_BIT
            | VK_SUBGROUP_FEATURE_SHUFFLE_BIT
            | VK_SUBGROUP_FEATURE_QUAD_BIT
            | VK_SUBGROUP_FEATURE_VOTE_BIT;
    static const VkSubgroupFeatureFlags required_stages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    if (!vulkan_test_context_init_instance(context, instance_extensions, ARRAY_SIZE(instance_extensions)))
        return false;

    if (!get_vulkan_queue_index(context, VK_QUEUE_GRAPHICS_BIT, &graphics_index))
    {
        skip("The selected Vulkan device does not support graphics operations.\n");
        goto out_destroy_context;
    }

    device_desc.pQueueCreateInfos = &queue_desc;
    device_desc.queueCreateInfoCount = 1;

    queue_desc.queueFamilyIndex = graphics_index;
    queue_desc.queueCount = 1;
    queue_desc.pQueuePriorities = &queue_priority;

    if (!check_device_extensions(runner, &enabled_extensions))
        goto out_destroy_context;
    device_desc.ppEnabledExtensionNames = enabled_extensions.names;
    device_desc.enabledExtensionCount = enabled_extensions.count;

    VK_CALL(vkGetPhysicalDeviceFormatProperties(context->phys_device, VK_FORMAT_R32G32B32A32_SFLOAT, &format_props));
    if (!(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        skip("The selected Vulkan device does not support R32G32B32A32_SFLOAT render targets.\n");
        goto out_destroy_context;
    }

    runner->caps.runner = "Vulkan";
    runner->caps.compiler = HLSL_COMPILER;
    get_physical_device_info(runner, &device_info);
    ret_features = &device_info.features2.features;

    runner->caps.tags[SHADER_RUNNER_TAG_VULKAN].present = true;

    runner->caps.tags[SHADER_RUNNER_TAG_MVK].present = get_mvk_driver_version(&device_info.properties2.properties,
            &device_info.driver_properties, &runner->caps.tags[SHADER_RUNNER_TAG_MVK].major,
            &runner->caps.tags[SHADER_RUNNER_TAG_MVK].minor, &runner->caps.tags[SHADER_RUNNER_TAG_MVK].patch);

    runner->caps.tags[SHADER_RUNNER_TAG_MESA].present = get_mesa_driver_version(&device_info.properties2.properties,
            &device_info.driver_properties, &runner->caps.tags[SHADER_RUNNER_TAG_MESA].major,
            &runner->caps.tags[SHADER_RUNNER_TAG_MESA].minor, &runner->caps.tags[SHADER_RUNNER_TAG_MESA].patch);

    runner->caps.tags[SHADER_RUNNER_TAG_LLVM].present = get_llvm_driver_version(
            &device_info.properties2.properties, &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].major,
            &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].minor, &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].patch);

    if (device_info.driver_properties.driverID == VK_DRIVER_ID_MESA_LLVMPIPE)
        runner->caps.tags[SHADER_RUNNER_TAG_LLVMPIPE].present = true;

    if (device_info.driver_properties.driverID == VK_DRIVER_ID_MESA_RADV_KHR)
        runner->caps.tags[SHADER_RUNNER_TAG_RADV].present = true;

    if (device_info.driver_properties.driverID == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA)
        runner->caps.tags[SHADER_RUNNER_TAG_INTEL].present = true;

    runner->caps.shader_caps[SHADER_CAP_CLIP_PLANES] = true;
    runner->caps.shader_caps[SHADER_CAP_FOG] = true;
    runner->caps.shader_caps[SHADER_CAP_POINT_SIZE] = true;

    subgroup_properties = &device_info.subgroup_properties;
    if (subgroup_properties->subgroupSize >= 4
            && (subgroup_properties->supportedOperations & required_subgroup_features) == required_subgroup_features
            && (subgroup_properties->supportedStages & required_stages) == required_stages)
        runner->caps.shader_caps[SHADER_CAP_WAVE_OPS] = true;

    device_desc.pEnabledFeatures = &features;
    memset(&features, 0, sizeof(features));

    /* FIXME: Probably make these optional. */

#define ENABLE_FEATURE(x) \
    do \
    { \
        if (!ret_features->x) \
        { \
            skip("The selected Vulkan device does not support " #x ".\n"); \
            goto out_destroy_context; \
        } \
        features.x = VK_TRUE; \
    } while (false)

    ENABLE_FEATURE(fragmentStoresAndAtomics);
    ENABLE_FEATURE(sampleRateShading);
    ENABLE_FEATURE(shaderClipDistance);
    ENABLE_FEATURE(shaderImageGatherExtended);
    ENABLE_FEATURE(shaderStorageImageWriteWithoutFormat);
    if (runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX])
        ENABLE_FEATURE(multiViewport);

    if (ret_features->tessellationShader)
    {
        features.tessellationShader = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_TESSELLATION_SHADER] = true;
    }

    if (ret_features->geometryShader)
    {
        features.geometryShader = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_GEOMETRY_SHADER] = true;
    }

    if (ret_features->shaderCullDistance)
    {
        features.shaderCullDistance = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_CULL_DISTANCE] = true;
    }

    if (ret_features->shaderFloat64)
    {
        features.shaderFloat64 = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_FLOAT64] = true;
    }

    if (ret_features->shaderInt64)
    {
        features.shaderInt64 = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_INT64] = true;
    }

    if (ret_features->depthBounds)
    {
        features.depthBounds = VK_TRUE;
        runner->caps.shader_caps[SHADER_CAP_DEPTH_BOUNDS] = true;
    }

    if (device_info.interlock_features.fragmentShaderSampleInterlock
            && device_info.interlock_features.fragmentShaderPixelInterlock)
    {
        memset(&interlock_features, 0, sizeof(interlock_features));
        interlock_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
        interlock_features.pNext = (void *)device_desc.pNext;
        interlock_features.fragmentShaderSampleInterlock = VK_TRUE;
        interlock_features.fragmentShaderPixelInterlock = VK_TRUE;
        device_desc.pNext = &interlock_features;
    }
    else
    {
        runner->caps.shader_caps[SHADER_CAP_ROV] = false;
    }

    if (device_info.demote_to_helper_invocation_features.shaderDemoteToHelperInvocation)
    {
        memset(&demote_to_helper_invocation_features, 0, sizeof(demote_to_helper_invocation_features));
        demote_to_helper_invocation_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT;
        demote_to_helper_invocation_features.pNext = (void *)device_desc.pNext;
        demote_to_helper_invocation_features.shaderDemoteToHelperInvocation = VK_TRUE;
        device_desc.pNext = &demote_to_helper_invocation_features;
    }
    else
    {
        runner->demote_to_helper_invocation = false;
    }

    if (device_info.features2.features.shaderStorageImageReadWithoutFormat)
        runner->caps.format_caps[DXGI_FORMAT_UNKNOWN] |= FORMAT_CAP_UAV_LOAD;
    for (unsigned int i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        runner->caps.format_caps[formats[i]] = get_format_support(context, &device_info, formats[i]);
    }

    b = vulkan_test_context_init_device(context, &device_desc, graphics_index, MAX_RESOURCES, MAX_SAMPLERS);
    free(enabled_extensions.names);

    if (b)
        return true;

out_destroy_context:
    vulkan_test_context_destroy(context);
    return false;
};

void run_shader_tests_vulkan(void *dxc_compiler)
{
    struct vulkan_shader_runner runner = {0};
    bool skip_sm2 = test_skipping_execution("Vulkan", HLSL_COMPILER, SHADER_MODEL_2_0, SHADER_MODEL_3_0);
    bool skip_sm4 = test_skipping_execution("Vulkan", HLSL_COMPILER, SHADER_MODEL_4_0, SHADER_MODEL_5_1);
    bool skip_sm6 = test_skipping_execution("Vulkan", "dxcompiler", SHADER_MODEL_6_0, SHADER_MODEL_MAX);

    if (skip_sm2 && skip_sm4 && (skip_sm6 || !dxc_compiler))
        return;

    if (!init_vulkan_runner(&runner))
        return;

    if (!skip_sm2)
    {
        runner.caps.minimum_shader_model = SHADER_MODEL_2_0;
        runner.caps.maximum_shader_model = SHADER_MODEL_3_0;
        run_shader_tests(&runner.r, &runner.caps, &vulkan_runner_ops, NULL);
    }

    /* Fog requires remapping, which is only correct for sm1. */
    runner.caps.shader_caps[SHADER_CAP_FOG] = false;

    if (!skip_sm4)
    {
        runner.caps.minimum_shader_model = SHADER_MODEL_4_0;
        runner.caps.maximum_shader_model = SHADER_MODEL_5_1;
        run_shader_tests(&runner.r, &runner.caps, &vulkan_runner_ops, NULL);
    }

    if (dxc_compiler && !skip_sm6)
    {
        runner.caps.compiler = "dxcompiler";
        runner.caps.minimum_shader_model = SHADER_MODEL_6_0;
        runner.caps.maximum_shader_model = SHADER_MODEL_MAX;
        run_shader_tests(&runner.r, &runner.caps, &vulkan_runner_ops, dxc_compiler);
    }

    vulkan_test_context_destroy(&runner.context);
}

#endif
