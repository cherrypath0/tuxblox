/*
 * Copyright 2021-2024 Elizabeth Figura for CodeWeavers
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

#ifdef _WIN32

#define COBJMACROS
#define CONST_VTABLE
#define VKD3D_TEST_NO_DEFS
#include <d3d9.h>
#include "vkd3d_d3dcommon.h"
#include "vkd3d_d3dcompiler.h"
#include "shader_runner.h"
#include "vkd3d_test.h"

struct d3d9_resource
{
    struct resource r;

    IDirect3DCubeTexture9 *cube;
    IDirect3DSurface9 *surface;
    IDirect3DTexture9 *texture;
    IDirect3DVertexBuffer9 *vb;
    IDirect3DVolumeTexture9 *volume;
};

static struct d3d9_resource *d3d9_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct d3d9_resource, r);
}

struct d3d9_shader_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;

    IDirect3DDevice9 *device;
    HWND window;
};

static struct d3d9_shader_runner *d3d9_shader_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct d3d9_shader_runner, r);
}

static IDirect3D9 *(WINAPI *pDirect3DCreate9)(UINT sdk_version);

static HRESULT set_viewport(IDirect3DDevice9 *device, unsigned int x, unsigned int y,
        unsigned int width, unsigned int height, float min_depth, float max_depth)
{
    D3DVIEWPORT9 vp =
    {
        .X = x,
        .Y = y,
        .Width = width,
        .Height = height,
        .MinZ = min_depth,
        .MaxZ = max_depth,
    };

    return IDirect3DDevice9_SetViewport(device, &vp);
}

static const BOOL windowed = TRUE;

static bool init_test_context(struct d3d9_shader_runner *runner)
{
    D3DPRESENT_PARAMETERS present_parameters =
    {
        .Windowed = windowed,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .BackBufferWidth = RENDER_TARGET_WIDTH,
        .BackBufferHeight = RENDER_TARGET_HEIGHT,
        .BackBufferFormat = D3DFMT_A8R8G8B8,
    };
    RECT rect = {0, 0, RENDER_TARGET_WIDTH, RENDER_TARGET_HEIGHT};
    D3DADAPTER_IDENTIFIER9 identifier;
    IDirect3D9 *d3d;
    D3DCAPS9 caps;
    HRESULT hr;

    memset(runner, 0, sizeof(*runner));

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    runner->window = CreateWindowA("static", "d3dcompiler_test", WS_OVERLAPPEDWINDOW,
            0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, NULL, NULL);
    ok(!!runner->window, "Failed to create a window.\n");

    d3d = pDirect3DCreate9(D3D_SDK_VERSION);
    ok(!!d3d, "Failed to create a D3D object.\n");

    hr = IDirect3D9_GetAdapterIdentifier(d3d, test_options.adapter_idx, 0, &identifier);
    ok(hr == S_OK, "Failed to get adapter identifier, hr %#lx.\n", hr);

    trace("Driver string: %s.\n", identifier.Driver);
    trace("Device: %s, %04lx:%04lx.\n", identifier.Description, identifier.VendorId, identifier.DeviceId);

    present_parameters.hDeviceWindow = runner->window;

    hr = IDirect3D9_CreateDevice(d3d, test_options.adapter_idx, D3DDEVTYPE_HAL, runner->window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &runner->device);
    IDirect3D9_Release(d3d);
    if (FAILED(hr))
    {
        skip("Failed to create a 3D device, hr %#lx.\n", hr);
        DestroyWindow(runner->window);
        return false;
    }

    hr = IDirect3DDevice9_GetDeviceCaps(runner->device, &caps);
    ok(hr == D3D_OK, "Failed to get device caps, hr %#lx.\n", hr);
    if (caps.PixelShaderVersion < D3DPS_VERSION(2, 0) || caps.VertexShaderVersion < D3DVS_VERSION(2, 0))
    {
        skip("No shader model 2 support.\n");
        IDirect3DDevice9_Release(runner->device);
        DestroyWindow(runner->window);
        return false;
    }

    runner->caps.runner = "d3d9.dll";
    runner->caps.compiler = HLSL_COMPILER;
    runner->caps.minimum_shader_model = SHADER_MODEL_2_0;
    runner->caps.maximum_shader_model = SHADER_MODEL_3_0;
    runner->caps.shader_caps[SHADER_CAP_CLIP_PLANES] = true;
    runner->caps.shader_caps[SHADER_CAP_FOG] = true;
    runner->caps.shader_caps[SHADER_CAP_POINT_SIZE] = true;

    runner->caps.tags[SHADER_RUNNER_TAG_WINDOWS].present = true;

    if (identifier.VendorId == 0x1414 && identifier.DeviceId == 0x008c)
    {
        trace("Using WARP device.\n");
        runner->caps.tags[SHADER_RUNNER_TAG_WARP].present = true;
    }
    else if (identifier.VendorId == 0x10de)
    {
        runner->caps.tags[SHADER_RUNNER_TAG_NVIDIA].present = true;
    }

    return true;
}

static void destroy_test_context(struct d3d9_shader_runner *runner)
{
    ULONG ref;

    ref = IDirect3DDevice9_Release(runner->device);
    ok(!ref, "Device has %lu references left.\n", ref);
    DestroyWindow(runner->window);
}

static D3DTEXTUREADDRESS sampler_address_to_d3d9(D3D12_TEXTURE_ADDRESS_MODE address)
{
    switch (address)
    {
        case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
            return D3DTADDRESS_WRAP;

        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
            return D3DTADDRESS_MIRROR;

        case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:
            return D3DTADDRESS_CLAMP;

        case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
            return D3DTADDRESS_BORDER;

        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE:
            return D3DTADDRESS_MIRRORONCE;
    }

    vkd3d_unreachable();
}

static D3DMULTISAMPLE_TYPE get_multisample_type(D3DFORMAT format, unsigned int sample_count)
{
    D3DMULTISAMPLE_TYPE ms_type;
    IDirect3D9 *d3d;
    HRESULT hr;

    if (sample_count == 1)
        return D3DMULTISAMPLE_NONE;

    assert(sample_count <= 16);
    ms_type = (D3DMULTISAMPLE_TYPE)sample_count;

    d3d = pDirect3DCreate9(D3D_SDK_VERSION);
    ok(!!d3d, "Failed to create a D3D object.\n");

    hr = IDirect3D9_CheckDeviceMultiSampleType(d3d, test_options.adapter_idx,
            D3DDEVTYPE_HAL, format, windowed, ms_type, NULL);
    IDirect3D9_Release(d3d);

    if (FAILED(hr))
        fatal_error("Format #%x with sample count %u is not supported.\n", format, sample_count);

    return ms_type;
}

static struct resource *d3d9_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct d3d9_shader_runner *runner = d3d9_shader_runner(r);
    IDirect3DDevice9 *device = runner->device;
    struct d3d9_resource *resource;
    D3DFORMAT format;
    HRESULT hr;
    void *data;

    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    switch (params->desc.format)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            format = D3DFMT_A32B32G32R32F;
            break;

        case DXGI_FORMAT_R32_FLOAT:
            format = D3DFMT_R32F;
            break;

        case DXGI_FORMAT_D32_FLOAT:
            format = D3DFMT_D32F_LOCKABLE;
            break;

        default:
            format = D3DFMT_UNKNOWN;
            break;
    }

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            hr = IDirect3DDevice9_CreateRenderTarget(device, params->desc.width, params->desc.height,
                    format, get_multisample_type(format, params->desc.sample_count),
                    0, FALSE, &resource->surface, NULL);
            ok(hr == D3D_OK, "Failed to create render target, hr %#lx.\n", hr);
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            hr = IDirect3DDevice9_CreateDepthStencilSurface(device, params->desc.width, params->desc.height,
                    format, get_multisample_type(format, params->desc.sample_count),
                    0, FALSE, &resource->surface, NULL);
            ok(hr == D3D_OK, "Failed to create depth surface, hr %#lx.\n", hr);
            break;

        case RESOURCE_TYPE_TEXTURE:
        {
            unsigned int src_buffer_offset = 0;

            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
            {
                fatal_error("Buffer resources are not supported.\n");
                break;
            }
            else if (params->desc.dimension == RESOURCE_DIMENSION_2D)
            {
                hr = IDirect3DDevice9_CreateTexture(device, params->desc.width, params->desc.height,
                        params->desc.level_count, 0, format, D3DPOOL_MANAGED, &resource->texture, NULL);
                ok(hr == D3D_OK, "Failed to create texture, hr %#lx.\n", hr);

                if (!params->data_size)
                    break;

                for (unsigned int level = 0; level < params->desc.level_count; ++level)
                {
                    unsigned int level_width = get_level_dimension(params->desc.width, level);
                    unsigned int level_height = get_level_dimension(params->desc.height, level);
                    unsigned int src_row_pitch = level_width * params->desc.texel_size;
                    unsigned int src_slice_pitch = level_height * src_row_pitch;
                    D3DLOCKED_RECT map_desc;

                    hr = IDirect3DTexture9_LockRect(resource->texture, level, &map_desc, NULL, 0);
                    ok(hr == D3D_OK, "Failed to map texture, hr %#lx.\n", hr);
                    for (unsigned int y = 0; y < level_height; ++y)
                        memcpy(&((uint8_t *)map_desc.pBits)[y * map_desc.Pitch],
                                &params->data[src_buffer_offset + y * src_row_pitch], src_row_pitch);
                    hr = IDirect3DTexture9_UnlockRect(resource->texture, level);
                    ok(hr == D3D_OK, "Failed to unmap texture, hr %#lx.\n", hr);

                    src_buffer_offset += src_slice_pitch;
                }
                break;
            }
            else if (params->desc.dimension == RESOURCE_DIMENSION_3D)
            {
                hr = IDirect3DDevice9_CreateVolumeTexture(device,
                        params->desc.width, params->desc.height, params->desc.depth,
                        params->desc.level_count, 0, format, D3DPOOL_MANAGED, &resource->volume, NULL);
                ok(hr == D3D_OK, "Failed to create texture, hr %#lx.\n", hr);

                for (unsigned int level = 0; level < params->desc.level_count; ++level)
                {
                    unsigned int level_width = get_level_dimension(params->desc.width, level);
                    unsigned int level_height = get_level_dimension(params->desc.height, level);
                    unsigned int level_depth = get_level_dimension(params->desc.depth, level);
                    unsigned int src_row_pitch = level_width * params->desc.texel_size;
                    unsigned int src_slice_pitch = level_height * src_row_pitch;
                    D3DLOCKED_BOX map_desc;

                    hr = IDirect3DVolumeTexture9_LockBox(resource->volume, level, &map_desc, NULL, 0);
                    ok(hr == D3D_OK, "Failed to map texture, hr %#lx.\n", hr);
                    for (unsigned int z = 0; z < level_depth; ++z)
                    {
                        for (unsigned int y = 0; y < level_height; ++y)
                            memcpy(&((uint8_t *)map_desc.pBits)[z * map_desc.SlicePitch + y * map_desc.RowPitch],
                                    &params->data[src_buffer_offset + z * src_slice_pitch + y * src_row_pitch],
                                    src_row_pitch);
                    }
                    hr = IDirect3DVolumeTexture9_UnlockBox(resource->volume, level);
                    ok(hr == D3D_OK, "Failed to unmap texture, hr %#lx.\n", hr);

                    src_buffer_offset += (src_slice_pitch * level_depth);
                }
                break;
            }
            else if (params->desc.dimension == RESOURCE_DIMENSION_CUBE)
            {
                hr = IDirect3DDevice9_CreateCubeTexture(device, params->desc.width,
                        params->desc.level_count, 0, format, D3DPOOL_MANAGED, &resource->cube, NULL);
                ok(hr == D3D_OK, "Failed to create texture, hr %#lx.\n", hr);

                for (unsigned int level = 0; level < params->desc.level_count; ++level)
                {
                    unsigned int level_width = get_level_dimension(params->desc.width, level);
                    unsigned int src_row_pitch = level_width * params->desc.texel_size;
                    unsigned int src_slice_pitch = level_width * src_row_pitch;
                    D3DLOCKED_RECT map_desc;

                    for (unsigned int face = 0; face < 6; ++face)
                    {
                        hr = IDirect3DCubeTexture9_LockRect(resource->cube, face, level, &map_desc, NULL, 0);
                        ok(hr == D3D_OK, "Failed to map texture, hr %#lx.\n", hr);
                        for (unsigned int y = 0; y < level_width; ++y)
                            memcpy(&((uint8_t *)map_desc.pBits)[y * map_desc.Pitch],
                                    &params->data[src_buffer_offset + y * src_row_pitch], src_row_pitch);
                        hr = IDirect3DCubeTexture9_UnlockRect(resource->cube, face, level);
                        ok(hr == D3D_OK, "Failed to unmap texture, hr %#lx.\n", hr);

                        src_buffer_offset += src_slice_pitch;
                    }
                }
                break;
            }
        }

        case RESOURCE_TYPE_UAV:
            fatal_error("UAVs are not supported.\n");
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            hr = IDirect3DDevice9_CreateVertexBuffer(device, params->data_size,
                    D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &resource->vb, NULL);
            ok(hr == D3D_OK, "Failed to create vertex buffer, hr %#lx.\n", hr);

            hr = IDirect3DVertexBuffer9_Lock(resource->vb, 0, 0, &data, D3DLOCK_DISCARD);
            ok(hr == D3D_OK, "Failed to map texture, hr %#lx.\n", hr);
            memcpy(data, params->data, params->data_size);
            hr = IDirect3DVertexBuffer9_Unlock(resource->vb);
            ok(hr == D3D_OK, "Failed to unmap texture, hr %#lx.\n", hr);
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    return &resource->r;
}

static void d3d9_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct d3d9_resource *resource = d3d9_resource(res);

    if (resource->cube)
        IDirect3DCubeTexture9_Release(resource->cube);
    if (resource->surface)
        IDirect3DSurface9_Release(resource->surface);
    if (resource->texture)
        IDirect3DTexture9_Release(resource->texture);
    if (resource->vb)
        IDirect3DVertexBuffer9_Release(resource->vb);
    if (resource->volume)
        IDirect3DVolumeTexture9_Release(resource->volume);
    free(resource);
}

static D3DDECLTYPE vertex_decl_type_from_format(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32_FLOAT:
            return D3DDECLTYPE_FLOAT1;

        case DXGI_FORMAT_R32G32_FLOAT:
            return D3DDECLTYPE_FLOAT2;

        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return D3DDECLTYPE_FLOAT4;

        default:
            fatal_error("Cannot translate format %#x to a d3d9 vertex buffer format.\n", format);
    }
}

static D3DDECLUSAGE vertex_decl_usage_from_name(const char *name)
{
    if (!strcasecmp(name, "color"))
        return D3DDECLUSAGE_COLOR;
    if (!strcasecmp(name, "position") || !strcasecmp(name, "sv_position"))
        return D3DDECLUSAGE_POSITION;
    if (!strcasecmp(name, "psize"))
        return D3DDECLUSAGE_PSIZE;
    if (!strcasecmp(name, "texcoord"))
        return D3DDECLUSAGE_TEXCOORD;
    fatal_error("Cannot translate usage \"%s\" to a d3d9 usage.\n", name);
}

static bool d3d9_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    fatal_error("Compute shaders are not supported.\n");
}

static uint32_t d3d_color_from_vec4(const struct vec4 *v)
{
    return vkd3d_make_u32(vkd3d_make_u16(v->z * 255.0f, v->y * 255.0f),
            vkd3d_make_u16(v->x * 255.0f, v->w * 255.0f));
}

static void d3d9_runner_clear(struct shader_runner *r, struct resource *res, const struct vec4 *clear_value)
{
    struct d3d9_shader_runner *runner = d3d9_shader_runner(r);
    struct d3d9_resource *resource = d3d9_resource(res);
    HRESULT hr;

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            hr = IDirect3DDevice9_ColorFill(runner->device, resource->surface,
                    NULL, d3d_color_from_vec4(clear_value));
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            hr = IDirect3DDevice9_SetDepthStencilSurface(runner->device, resource->surface);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            hr = IDirect3DDevice9_Clear(runner->device, 0, NULL, D3DCLEAR_ZBUFFER, 0, clear_value->x, 0);
            ok(hr == S_OK, "Got hr %#lx.\n", hr);
            break;

        default:
            fatal_error("Clears are not implemented for resource type %u.\n", resource->r.desc.type);
    }
}

static bool d3d9_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY primitive_topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    static const D3DVERTEXELEMENT9 decl_element_end = D3DDECL_END();
    struct d3d9_shader_runner *runner = d3d9_shader_runner(r);
    IDirect3DVertexDeclaration9 *vertex_declaration;
    IDirect3DDevice9 *device = runner->device;
    unsigned int fb_width, fb_height, i, j;
    D3DVERTEXELEMENT9 *decl_elements;
    const struct uniform_buffer *u;
    ID3D10Blob *vs_code, *ps_code;
    IDirect3DVertexShader9 *vs;
    IDirect3DPixelShader9 *ps;
    D3DMATRIX proj_matrix;
    HRESULT hr;

    if (instance_count > 1)
        fatal_error("Unhandled instance count %u.\n", instance_count);
    if (first_vertex)
        fatal_error("Unhandled first vertex %u.\n", first_vertex);
    if (first_instance)
        fatal_error("Unhandled first instance %u.\n", first_instance);
    if (r->viewport_count)
        fatal_error("Unhandled viewports.\n");

    if (!(vs_code = compile_hlsl(&runner->r, SHADER_TYPE_VS)))
        return false;

    if (!(ps_code = compile_hlsl(&runner->r, SHADER_TYPE_PS)))
    {
        ID3D10Blob_Release(vs_code);
        return false;
    }

    u = &runner->r.uniform_buffers[VKD3D_SHADER_D3DBC_FLOAT_CONSTANT_REGISTER];
    if (u->count)
    {
        hr = IDirect3DDevice9_SetVertexShaderConstantF(device, 0, (const float *)u->elements, u->count / 4);
        ok(hr == D3D_OK, "Failed to set uniforms, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetPixelShaderConstantF(device, 0, (const float *)u->elements, u->count / 4);
        ok(hr == D3D_OK, "Failed to set uniforms, hr %#lx.\n", hr);
    }

    u = &runner->r.uniform_buffers[VKD3D_SHADER_D3DBC_INT_CONSTANT_REGISTER];
    if (u->count)
    {
        hr = IDirect3DDevice9_SetVertexShaderConstantI(device, 0, (const int *)u->elements, u->count / 4);
        ok(hr == D3D_OK, "Failed to set integer constants, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetPixelShaderConstantI(device, 0, (const int *)u->elements, u->count / 4);
        ok(hr == D3D_OK, "Failed to set integer constants, hr %#lx.\n", hr);
    }

    decl_elements = calloc(runner->r.input.element_count + 1, sizeof(*decl_elements));
    for (i = 0; i < runner->r.input.element_count; ++i)
    {
        const struct input_element *src_element = &runner->r.input.elements[i];
        D3DVERTEXELEMENT9 *dst_element = &decl_elements[i];

        dst_element->Stream = src_element->slot;
        dst_element->Type = vertex_decl_type_from_format(src_element->format);
        dst_element->Method = D3DDECLMETHOD_DEFAULT;
        dst_element->Usage = vertex_decl_usage_from_name(src_element->name);
        dst_element->UsageIndex = src_element->index;

        /* The offset will be filled below. */
    }
    decl_elements[runner->r.input.element_count] = decl_element_end;

    fb_width = ~0u;
    fb_height = ~0u;
    for (i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d9_resource *resource = d3d9_resource(runner->r.resources[i]);
        unsigned int stride = 0;

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
                hr = IDirect3DDevice9_SetRenderTarget(device, resource->r.desc.slot, resource->surface);
                ok(hr == D3D_OK, "Failed to set render target, hr %#lx.\n", hr);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_DEPTH_STENCIL:
                hr = IDirect3DDevice9_SetDepthStencilSurface(device, resource->surface);
                ok(hr == D3D_OK, "Failed to set depth surface, hr %#lx.\n", hr);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_TEXTURE:
                assert(resource->r.desc.dimension != RESOURCE_DIMENSION_BUFFER);

                if (resource->r.desc.dimension == RESOURCE_DIMENSION_2D)
                    hr = IDirect3DDevice9_SetTexture(device, resource->r.desc.slot,
                            (IDirect3DBaseTexture9 *)resource->texture);
                else if (resource->r.desc.dimension == RESOURCE_DIMENSION_3D)
                    hr = IDirect3DDevice9_SetTexture(device, resource->r.desc.slot,
                            (IDirect3DBaseTexture9 *)resource->volume);
                else
                    hr = IDirect3DDevice9_SetTexture(device, resource->r.desc.slot,
                            (IDirect3DBaseTexture9 *)resource->cube);
                ok(hr == D3D_OK, "Failed to set texture, hr %#lx.\n", hr);
                break;

            case RESOURCE_TYPE_UAV:
                vkd3d_unreachable();

            case RESOURCE_TYPE_VERTEX_BUFFER:
                for (j = 0; j < runner->r.input.element_count; ++j)
                {
                    if (runner->r.input.elements[j].slot == resource->r.desc.slot)
                    {
                        decl_elements[j].Offset = stride;
                        stride += runner->r.input.elements[j].texel_size;
                    }
                }

                hr = IDirect3DDevice9_SetStreamSource(device, resource->r.desc.slot, resource->vb, 0, stride);
                ok(hr == D3D_OK, "Failed to set vertex buffer, hr %#lx.\n", hr);
                break;

            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }

    hr = set_viewport(device, 0, 0, fb_width, fb_height, 0.0f, 1.0f);
    ok(hr == D3D_OK, "Failed to set viewport, hr %#lx.\n", hr);

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        const struct sampler *sampler = &runner->r.samplers[i];

        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot,
                D3DSAMP_ADDRESSU, sampler_address_to_d3d9(sampler->u_address));
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot,
                D3DSAMP_ADDRESSV, sampler_address_to_d3d9(sampler->v_address));
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot,
                D3DSAMP_ADDRESSW, sampler_address_to_d3d9(sampler->w_address));
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);

        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot, D3DSAMP_MINFILTER,
                (sampler->filter & 0x10) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot, D3DSAMP_MAGFILTER,
                (sampler->filter & 0x4) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetSamplerState(device, sampler->slot, D3DSAMP_MIPFILTER,
                (sampler->filter & 0x1) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        ok(hr == D3D_OK, "Failed to set sampler state, hr %#lx.\n", hr);
    }

    /* D3DCMPFUNC is compatible with D3D12_COMPARISON_FUNC. */
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ZFUNC, runner->r.depth_func);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, TRUE);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHATESTENABLE, TRUE);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    /* The members of enum vkd3d_shader_parameter_alpha_test_func are
     * compatible with D3DCMPFUNC. */
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHAFUNC, runner->r.alpha_test_func);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHAREF, runner->r.alpha_test_ref * 255.0);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_SHADEMODE,
            runner->r.flat_shading ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CLIPPING, TRUE);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CLIPPLANEENABLE, runner->r.clip_plane_mask);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    for (unsigned int i = 0; i < 8; ++i)
    {
        hr = IDirect3DDevice9_SetClipPlane(device, i, &runner->r.clip_planes[i].x);
        ok(hr == D3D_OK, "Failed to set clip plane, hr %#lx.\n", hr);
    }

    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_POINTSIZE, float_to_int(runner->r.point_size));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_POINTSIZE_MIN, float_to_int(runner->r.point_size_min));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_POINTSIZE_MAX, float_to_int(runner->r.point_size_max));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_POINTSPRITEENABLE, runner->r.point_sprite);
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGENABLE, (runner->r.fog_mode != FOG_MODE_DISABLE));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    if (runner->r.fog_mode != FOG_MODE_DISABLE)
    {
        hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGTABLEMODE, runner->r.fog_mode);
        ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    }
    memset(&proj_matrix, 0, sizeof(proj_matrix));
    proj_matrix._11 = proj_matrix._22 = proj_matrix._33 = proj_matrix._44 = 1.0f;
    if (!runner->r.ortho_fog)
        proj_matrix._44 = 1.01f;
    hr = IDirect3DDevice9_SetTransform(device, D3DTS_PROJECTION, &proj_matrix);
    ok(hr == D3D_OK, "Failed to set projection matrix, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGCOLOR, d3d_color_from_vec4(&runner->r.fog_colour));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGSTART, float_to_int(runner->r.fog_start));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGEND, float_to_int(runner->r.fog_end));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_FOGDENSITY, float_to_int(runner->r.fog_density));
    ok(hr == D3D_OK, "Failed to set render state, hr %#lx.\n", hr);

    for (unsigned int i = 0; i < 8; ++i)
    {
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVMAT00,
                float_to_int(runner->r.bump[i].matrix[0][0]));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVMAT01,
                float_to_int(runner->r.bump[i].matrix[0][1]));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVMAT10,
                float_to_int(runner->r.bump[i].matrix[1][0]));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVMAT11,
                float_to_int(runner->r.bump[i].matrix[1][1]));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVLSCALE,
                float_to_int(runner->r.bump[i].luminance_scale));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_BUMPENVLOFFSET,
                float_to_int(runner->r.bump[i].luminance_offset));
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);

        hr = IDirect3DDevice9_SetTextureStageState(device, i, D3DTSS_TEXTURETRANSFORMFLAGS,
                (runner->r.projected_texture_mask & (1u << i)) ? D3DTTFF_PROJECTED : 0);
        ok(hr == D3D_OK, "Failed to set texture state, hr %#lx.\n", hr);
    }

    hr = IDirect3DDevice9_CreateVertexDeclaration(device, decl_elements, &vertex_declaration);
    ok(hr == D3D_OK, "Failed to create vertex declaration, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_code), &vs);
    ok(hr == D3D_OK, "Failed to create vertex shader, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_code), &ps);
    ok(hr == D3D_OK, "Failed to create pixel shader, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_SetVertexDeclaration(device, vertex_declaration);
    ok(hr == D3D_OK, "Failed to set vertex declaration, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetVertexShader(device, vs);
    ok(hr == D3D_OK, "Failed to set vertex shader, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetPixelShader(device, ps);
    ok(hr == D3D_OK, "Failed to set pixel shader, hr %#lx.\n", hr);
    hr = IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE);
    ok(hr == D3D_OK, "Failed to set cull mode, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_BeginScene(device);
    ok(hr == D3D_OK, "Failed to draw, hr %#lx.\n", hr);

    switch (primitive_topology)
    {
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
            hr = IDirect3DDevice9_DrawPrimitive(device, D3DPT_POINTLIST, 0, vertex_count);
            break;

        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            hr = IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLELIST, 0, vertex_count / 3);
            break;

        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
            hr = IDirect3DDevice9_DrawPrimitive(device, D3DPT_TRIANGLESTRIP, 0, vertex_count - 2);
            break;

        default:
            fatal_error("Cannot translate topology %#x to a d3d9 topology.\n", primitive_topology);
    }
    ok(hr == D3D_OK, "Failed to draw, hr %#lx.\n", hr);

    hr = IDirect3DDevice9_EndScene(device);
    ok(hr == D3D_OK, "Failed to draw, hr %#lx.\n", hr);

    IDirect3DVertexDeclaration9_Release(vertex_declaration);
    IDirect3DVertexShader9_Release(vs);
    IDirect3DPixelShader9_Release(ps);

    return true;
}

struct d3d9_resource_readback
{
    struct resource_readback rb;
    IDirect3DSurface9 *surface;
};

static struct resource_readback *d3d9_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct d3d9_shader_runner *runner = d3d9_shader_runner(r);
    struct d3d9_resource_readback *rb = malloc(sizeof(*rb));
    struct d3d9_resource *resource = d3d9_resource(res);
    IDirect3DSurface9 *surface = resource->surface;
    IDirect3DSurface9 *resolved_surface = NULL;
    D3DLOCKED_RECT map_desc;
    D3DSURFACE_DESC desc;
    HRESULT hr;

    if (sub_resource_idx)
        fatal_error("Unsupported sub-resource index %u.\n", sub_resource_idx);

    hr = IDirect3DSurface9_GetDesc(surface, &desc);
    ok(hr == D3D_OK, "Failed to get surface desc, hr %#lx.\n", hr);

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_DEPTH_STENCIL:
            rb->surface = resource->surface;
            IDirect3DSurface9_AddRef(rb->surface);
            break;

        case RESOURCE_TYPE_RENDER_TARGET:
            hr = IDirect3DDevice9Ex_CreateOffscreenPlainSurface(runner->device, desc.Width,
                    desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &rb->surface, NULL);
            ok(hr == D3D_OK, "Failed to create surface, hr %#lx.\n", hr);

            if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
            {
                hr = IDirect3DDevice9_CreateRenderTarget(runner->device, desc.Width, desc.Height, desc.Format,
                        D3DMULTISAMPLE_NONE, 0, FALSE, &resolved_surface, NULL);
                ok(hr == D3D_OK, "Failed to create resolved render target, hr %#lx.\n", hr);

                hr = IDirect3DDevice9_StretchRect(runner->device, surface, NULL, resolved_surface, NULL, D3DTEXF_NONE);
                ok(hr == D3D_OK, "Failed to resolve multisampled surface, hr %#lx.\n", hr);

                surface = resolved_surface;
            }

            hr = IDirect3DDevice9Ex_GetRenderTargetData(runner->device, surface, rb->surface);
            ok(hr == D3D_OK, "Failed to get render target data, hr %#lx.\n", hr);

            if (resolved_surface)
                IDirect3DSurface9_Release(resolved_surface);
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UAV:
        case RESOURCE_TYPE_UNIFORM_BUFFER:
            assert(0);
    }

    hr = IDirect3DSurface9_LockRect(rb->surface, &map_desc, NULL, D3DLOCK_READONLY);
    ok(hr == D3D_OK, "Failed to lock surface, hr %#lx.\n", hr);

    rb->rb.data = map_desc.pBits;
    rb->rb.row_pitch = map_desc.Pitch;
    rb->rb.width = desc.Width;
    rb->rb.height = desc.Height;
    rb->rb.depth = 1;
    return &rb->rb;
}

static void d3d9_runner_release_readback(struct shader_runner *r, struct resource_readback *rb)
{
    struct d3d9_resource_readback *d3d9_rb = CONTAINING_RECORD(rb, struct d3d9_resource_readback, rb);

    IDirect3DSurface9_UnlockRect(d3d9_rb->surface);
    IDirect3DSurface9_Release(d3d9_rb->surface);
    free(d3d9_rb);
}

static bool d3d9_runner_copy(struct shader_runner *r, struct resource *s, struct resource *d)
{
    struct d3d9_resource *src = d3d9_resource(s);
    struct d3d9_resource *dst = d3d9_resource(d);

    if (src->r.desc.type == RESOURCE_TYPE_RENDER_TARGET && dst->r.desc.type == RESOURCE_TYPE_TEXTURE)
    {
        /* We create our textures with MANAGED, and unfortunately there's no
         * way to directly blit into a managed texture. */
        struct resource_readback *rb;
        D3DLOCKED_RECT map_desc;
        HRESULT hr;

        rb = d3d9_runner_get_resource_readback(r, s, 0);

        hr = IDirect3DTexture9_LockRect(dst->texture, 0, &map_desc, NULL, 0);
        ok(hr == S_OK, "Failed to map texture, hr %#lx.\n", hr);
        for (unsigned int y = 0; y < src->r.desc.height; ++y)
            memcpy(&((char *)map_desc.pBits)[y * map_desc.Pitch], &((char *)rb->data)[y * rb->row_pitch],
                    src->r.desc.width * src->r.desc.texel_size);
        hr = IDirect3DTexture9_UnlockRect(dst->texture, 0);
        ok(hr == S_OK, "Failed to unmap texture, hr %#lx.\n", hr);

        d3d9_runner_release_readback(r, rb);

        return true;
    }

    return false;
}

static const struct shader_runner_ops d3d9_runner_ops =
{
    .create_resource = d3d9_runner_create_resource,
    .destroy_resource = d3d9_runner_destroy_resource,
    .dispatch = d3d9_runner_dispatch,
    .clear = d3d9_runner_clear,
    .draw = d3d9_runner_draw,
    .copy = d3d9_runner_copy,
    .get_resource_readback = d3d9_runner_get_resource_readback,
    .release_readback = d3d9_runner_release_readback,
};

void run_shader_tests_d3d9(void)
{
    struct d3d9_shader_runner runner;
    HMODULE d3d9_module;

    if (test_skipping_execution("d3d9.dll",
            HLSL_COMPILER, SHADER_MODEL_2_0, SHADER_MODEL_3_0))
        return;

    if (!(d3d9_module = LoadLibraryA("d3d9.dll")))
        return;

    pDirect3DCreate9 = (void *)GetProcAddress(d3d9_module, "Direct3DCreate9");

    if (init_test_context(&runner))
    {
        run_shader_tests(&runner.r, &runner.caps, &d3d9_runner_ops, NULL);
        destroy_test_context(&runner);
    }

    FreeLibrary(d3d9_module);
}

#endif
