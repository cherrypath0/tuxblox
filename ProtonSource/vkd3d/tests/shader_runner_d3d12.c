/*
 * Copyright 2020-2021 Zebediah Figura for CodeWeavers
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
#include <assert.h>
#ifndef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#define COBJMACROS
#define CONST_VTABLE
#define VKD3D_TEST_NO_DEFS
#include "d3d12_crosstest.h"
#include "shader_runner.h"
#include "dxcompiler.h"

VKD3D_AGILITY_SDK_EXPORTS

struct d3d12_resource
{
    struct resource r;

    D3D12_DESCRIPTOR_RANGE descriptor_range;
    ID3D12Resource *resource;
    unsigned int root_index;
};

static struct d3d12_resource *d3d12_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct d3d12_resource, r);
}

struct d3d12_shader_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;

    struct test_context test_context;

    ID3D12DescriptorHeap *heap, *rtv_heap, *dsv_heap;

    ID3D12CommandQueue *compute_queue;
    ID3D12CommandAllocator *compute_allocator;
    ID3D12GraphicsCommandList *compute_list;
};

static struct d3d12_shader_runner *d3d12_shader_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct d3d12_shader_runner, r);
}

#define MAX_RESOURCE_DESCRIPTORS (MAX_RESOURCES * 2)

static D3D12_RESOURCE_STATES resource_get_state(struct resource *r)
{
    if (r->desc.type == RESOURCE_TYPE_RENDER_TARGET)
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (r->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (r->desc.type == RESOURCE_TYPE_TEXTURE)
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (r->desc.type == RESOURCE_TYPE_UAV)
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    return D3D12_RESOURCE_STATE_GENERIC_READ;
}

static struct resource *d3d12_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *test_context = &runner->test_context;
    ID3D12Device *device = test_context->device;
    D3D12_RESOURCE_STATES initial_state, state;
    D3D12_SUBRESOURCE_DATA *resource_data;
    struct d3d12_resource *resource;
    unsigned int buffer_offset = 0;

    resource_data = calloc(params->desc.layer_count * params->desc.level_count, sizeof(*resource_data));
    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    for (unsigned int level = 0; level < params->desc.level_count; ++level)
    {
        unsigned int level_width = get_level_dimension(params->desc.width, level);
        unsigned int level_height = get_level_dimension(params->desc.height, level);
        unsigned int level_depth = get_level_dimension(params->desc.depth, level);

        for (unsigned int layer = 0; layer < params->desc.layer_count; ++layer)
        {
            D3D12_SUBRESOURCE_DATA *subresource = &resource_data[level + layer * params->desc.level_count];
            subresource->pData = &params->data[buffer_offset];
            subresource->RowPitch = level_width * params->desc.texel_size;
            subresource->SlicePitch = level_height * subresource->RowPitch;
            buffer_offset += level_depth * subresource->SlicePitch;
        }
    }

    state = resource_get_state(&resource->r);
    initial_state = params->data ? D3D12_RESOURCE_STATE_COPY_DEST : state;

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            if (!runner->rtv_heap)
                runner->rtv_heap = create_cpu_descriptor_heap(device,
                        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RESOURCE_DESCRIPTORS);

            if (params->desc.slot >= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
                fatal_error("RTV slot %u is too high.\n", params->desc.slot);
            if (params->desc.sample_count > 1 && params->desc.level_count > 1)
                fatal_error("Multisampled texture has multiple levels.\n");

            resource->resource = create_default_texture_(__FILE__, __LINE__, device,
                    D3D12_RESOURCE_DIMENSION_TEXTURE2D, params->desc.width, params->desc.height,
                    params->desc.layer_count, params->desc.level_count, params->desc.sample_count,
                    params->desc.format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, initial_state);
            ID3D12Device_CreateRenderTargetView(device, resource->resource,
                    NULL, get_cpu_rtv_handle(test_context, runner->rtv_heap, resource->r.desc.slot));
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            if (!runner->dsv_heap)
                runner->dsv_heap = create_cpu_descriptor_heap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

            resource->resource = create_default_texture2d(device, params->desc.width,
                    params->desc.height, params->desc.depth, params->desc.level_count,
                    params->desc.format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, initial_state);
            ID3D12Device_CreateDepthStencilView(device, resource->resource,
                    NULL, get_cpu_dsv_handle(test_context, runner->dsv_heap, 0));
            break;

        case RESOURCE_TYPE_TEXTURE:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
            {
                resource->resource = create_default_buffer(device, params->data_size, 0, initial_state);
                if (params->data)
                {
                    upload_buffer_data_with_states(resource->resource, 0, params->data_size, resource_data[0].pData,
                            test_context->queue, test_context->list, RESOURCE_STATE_DO_NOT_CHANGE, state);
                    reset_command_list(test_context->list, test_context->allocator);
                }
            }
            else
            {
                D3D12_RESOURCE_DIMENSION dimension;
                unsigned int depth;

                if (params->desc.sample_count > 1 && params->desc.level_count > 1)
                    fatal_error("Multisampled texture has multiple levels.\n");

                if (params->desc.dimension == RESOURCE_DIMENSION_3D)
                {
                    dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                    depth = params->desc.depth;
                }
                else
                {
                    dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                    depth = params->desc.layer_count;
                }

                resource->resource = create_default_texture_(__FILE__, __LINE__, device,
                        dimension, params->desc.width, params->desc.height, depth,
                        params->desc.level_count, params->desc.sample_count, params->desc.format,
                        /* Multisampled textures must have ALLOW_RENDER_TARGET set. */
                        (params->desc.sample_count > 1) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : 0, initial_state);
                if (params->data)
                {
                    if (params->desc.sample_count > 1)
                        fatal_error("Cannot upload data to a multisampled texture.\n");
                    upload_texture_data_with_states(resource->resource, resource_data,
                            params->desc.level_count * params->desc.layer_count,
                            test_context->queue, test_context->list, RESOURCE_STATE_DO_NOT_CHANGE, state);
                    reset_command_list(test_context->list, test_context->allocator);
                }
            }
            break;

        case RESOURCE_TYPE_UAV:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
            {
                resource->resource = create_default_buffer(device, params->data_size,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, initial_state);
                if (params->data)
                {
                    upload_buffer_data_with_states(resource->resource, 0, params->data_size, resource_data[0].pData,
                            test_context->queue, test_context->list, RESOURCE_STATE_DO_NOT_CHANGE, state);
                    reset_command_list(test_context->list, test_context->allocator);
                }
            }
            else
            {
                D3D12_RESOURCE_DIMENSION dimension;
                unsigned int depth;

                if (params->desc.dimension == RESOURCE_DIMENSION_2D)
                {
                    dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                    depth = params->desc.layer_count;
                }
                else
                {
                    dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                    depth = params->desc.depth;
                }

                resource->resource = create_default_texture_(__FILE__, __LINE__, device,
                        dimension, params->desc.width, params->desc.height, depth,
                        params->desc.level_count, 1, params->desc.format,
                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, initial_state);

                if (params->data)
                {
                    upload_texture_data_with_states(resource->resource, resource_data,
                            params->desc.level_count * params->desc.layer_count,
                            test_context->queue, test_context->list, RESOURCE_STATE_DO_NOT_CHANGE, state);
                    reset_command_list(test_context->list, test_context->allocator);
                }
            }
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            resource->resource = create_upload_buffer(device, params->data_size, params->data);
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    free(resource_data);
    return &resource->r;
}

static void d3d12_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct d3d12_resource *resource = d3d12_resource(res);

    ID3D12Resource_Release(resource->resource);
    free(resource);
}

static const D3D12_DESCRIPTOR_RANGE_TYPE range_types[] =
{
    [VKD3D_SHADER_DESCRIPTOR_TYPE_SRV]     = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    [VKD3D_SHADER_DESCRIPTOR_TYPE_UAV]     = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
    [VKD3D_SHADER_DESCRIPTOR_TYPE_CBV]     = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
    [VKD3D_SHADER_DESCRIPTOR_TYPE_SAMPLER] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
};

static ID3D12RootSignature *d3d12_runner_create_root_signature(struct d3d12_shader_runner *runner,
        ID3D12CommandQueue *queue, ID3D12CommandAllocator *allocator,
        ID3D12GraphicsCommandList *command_list, unsigned int *uniform_base)
{
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {0};
    D3D12_DESCRIPTOR_RANGE root_ranges[MAX_DESCRIPTORS];
    D3D12_ROOT_PARAMETER root_params[17], *root_param;
    D3D12_STATIC_SAMPLER_DESC static_samplers[7];
    ID3D12RootSignature *root_signature;
    unsigned int descriptor_index = 0;
    HRESULT hr;
    size_t i;

    root_signature_desc.NumParameters = 0;
    root_signature_desc.pParameters = root_params;
    root_signature_desc.NumStaticSamplers = 0;
    root_signature_desc.pStaticSamplers = static_samplers;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    if (runner->r.minimum_shader_model >= SHADER_MODEL_6_6)
        root_signature_desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

    /* Native d3d12 doesn't like an empty root descriptor table. */
    if (runner->r.descriptor_count)
    {
        root_param = &root_params[root_signature_desc.NumParameters++];

        root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_param->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        root_param->DescriptorTable.NumDescriptorRanges = 0;
        root_param->DescriptorTable.pDescriptorRanges = root_ranges;

        for (i = 0; i < runner->r.descriptor_count; ++i)
        {
            D3D12_DESCRIPTOR_RANGE *range = &root_ranges[root_param->DescriptorTable.NumDescriptorRanges++];
            struct descriptor_mapping *descriptor = &runner->r.descriptors[i];

            range->RangeType = range_types[descriptor->type];
            range->BaseShaderRegister = descriptor->register_idx;
            range->RegisterSpace = descriptor->register_space;
            range->NumDescriptors = descriptor->count;
            range->OffsetInDescriptorsFromTableStart = descriptor_index;
            descriptor_index += descriptor->count;
        }
    }

    *uniform_base = root_signature_desc.NumParameters;
    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        root_param = &root_params[root_signature_desc.NumParameters++];
        root_param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_param->Constants.ShaderRegister = i;
        root_param->Constants.RegisterSpace = 0;
        root_param->Constants.Num32BitValues = u->count;
        root_param->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    assert(root_signature_desc.NumParameters <= ARRAY_SIZE(root_params));

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        D3D12_STATIC_SAMPLER_DESC *sampler_desc = &static_samplers[root_signature_desc.NumStaticSamplers++];
        const struct sampler *sampler = &runner->r.samplers[i];

        memset(sampler_desc, 0, sizeof(*sampler_desc));
        sampler_desc->Filter = sampler->filter;
        sampler_desc->AddressU = sampler->u_address;
        sampler_desc->AddressV = sampler->v_address;
        sampler_desc->AddressW = sampler->w_address;
        sampler_desc->ComparisonFunc = sampler->func;
        sampler_desc->MaxLOD = FLT_MAX;
        sampler_desc->ShaderRegister = sampler->slot;
        sampler_desc->RegisterSpace = 0;
        sampler_desc->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    hr = create_root_signature(runner->test_context.device, &root_signature_desc, &root_signature);
    ok(hr == S_OK, "Failed to create root signature, hr %#x.\n", hr);
    return root_signature;
}

static struct d3d12_resource *find_resource(struct d3d12_shader_runner *runner,
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

    return d3d12_resource(resource);
}

static void write_descriptors(struct d3d12_shader_runner *runner)
{
    ID3D12Device *device = runner->test_context.device;
    unsigned int descriptor_index = 0, i, j;

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *mapping = &runner->r.descriptors[i];

        for (j = 0; j < mapping->count; ++j)
        {
            const struct d3d12_resource *resource = find_resource(runner, mapping, j);

            D3D12_CPU_DESCRIPTOR_HANDLE handle = get_cpu_descriptor_handle(&runner->test_context,
                    runner->heap, descriptor_index++);

            switch (mapping->type)
            {
                case VKD3D_SHADER_DESCRIPTOR_TYPE_SRV:
                    if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = { 0 };

                        srv_desc.Format = resource->r.desc.format;
                        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srv_desc.Buffer.NumElements = resource->r.desc.width * resource->r.desc.height;
                        srv_desc.Buffer.StructureByteStride = resource->r.stride;
                        srv_desc.Buffer.Flags = resource->r.is_raw ? D3D12_BUFFER_SRV_FLAG_RAW : 0;

                        ID3D12Device_CreateShaderResourceView(device, resource->resource, &srv_desc, handle);
                    }
                    else if (resource->r.desc.dimension == RESOURCE_DIMENSION_CUBE)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc =
                        {
                            .Format = resource->r.desc.format,
                            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
                            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                            .TextureCube.MostDetailedMip = 0,
                            .TextureCube.MipLevels = resource->r.desc.level_count,
                            .TextureCube.ResourceMinLODClamp = resource->r.desc.min_lod_clamp,
                        };

                        ID3D12Device_CreateShaderResourceView(device, resource->resource, &srv_desc, handle);
                    }
                    else if (resource->r.desc.dimension == RESOURCE_DIMENSION_2D
                            && resource->r.desc.layer_count == 1 && resource->r.desc.sample_count == 0)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc =
                        {
                            .Format = resource->r.desc.format,
                            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                            .Texture2D.MostDetailedMip = 0,
                            .Texture2D.MipLevels = resource->r.desc.level_count,
                            .Texture2D.PlaneSlice = 0,
                            .Texture2D.ResourceMinLODClamp = resource->r.desc.min_lod_clamp,
                        };

                        ID3D12Device_CreateShaderResourceView(device, resource->resource, &srv_desc, handle);
                    }
                    else if (resource->r.desc.dimension == RESOURCE_DIMENSION_2D && resource->r.desc.sample_count == 0)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc =
                        {
                            .Format = resource->r.desc.format,
                            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
                            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                            .Texture2DArray.MostDetailedMip = 0,
                            .Texture2DArray.MipLevels = resource->r.desc.level_count,
                            .Texture2DArray.FirstArraySlice = 0,
                            .Texture2DArray.ArraySize = resource->r.desc.layer_count,
                            .Texture2DArray.PlaneSlice = 0,
                            .Texture2DArray.ResourceMinLODClamp = resource->r.desc.min_lod_clamp,
                        };

                        ID3D12Device_CreateShaderResourceView(device, resource->resource, &srv_desc, handle);
                    }
                    else
                    {
                        ID3D12Device_CreateShaderResourceView(device, resource->resource, NULL, handle);
                    }
                    break;

                case VKD3D_SHADER_DESCRIPTOR_TYPE_UAV:
                    if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
                    {
                        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = { 0 };

                        uav_desc.Format = resource->r.desc.format;
                        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                        uav_desc.Buffer.NumElements = resource->r.desc.width * resource->r.desc.height;
                        uav_desc.Buffer.StructureByteStride = resource->r.stride;
                        uav_desc.Buffer.Flags = resource->r.is_raw ? D3D12_BUFFER_UAV_FLAG_RAW : 0;

                        ID3D12Device_CreateUnorderedAccessView(device, resource->resource,
                                resource->r.is_uav_counter ? resource->resource : NULL, &uav_desc, handle);
                    }
                    else
                    {
                        ID3D12Device_CreateUnorderedAccessView(device, resource->resource, NULL, NULL, handle);
                    }
                    break;

                default:
                    fatal_error("Invalid descriptor type %#x.\n", mapping->type);
            }
        }
    }
}

static bool d3d12_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *test_context = &runner->test_context;

    ID3D12GraphicsCommandList *command_list = runner->compute_list;
    ID3D12CommandAllocator *allocator = runner->compute_allocator;
    ID3D12CommandQueue *queue = runner->compute_queue;
    ID3D12Device *device = test_context->device;
    ID3D12RootSignature *root_signature;
    unsigned int uniform_index, i;
    ID3D12PipelineState *pso;
    D3D12_SHADER_BYTECODE cs;
    ID3D10Blob *cs_code;
    HRESULT hr;

    cs_code = compile_hlsl(&runner->r, SHADER_TYPE_CS);
    todo_if(runner->r.is_todo && runner->r.shader_model < SHADER_MODEL_6_0) ok(cs_code, "Failed to compile shader.\n");
    if (!cs_code)
        return false;

    root_signature = d3d12_runner_create_root_signature(runner, queue, allocator, command_list, &uniform_index);

    write_descriptors(runner);

    cs.pShaderBytecode = ID3D10Blob_GetBufferPointer(cs_code);
    cs.BytecodeLength = ID3D10Blob_GetBufferSize(cs_code);
    todo_if(runner->r.is_todo) bug_if(runner->r.is_bug)
    pso = create_compute_pipeline_state(device, root_signature, cs);
    ID3D10Blob_Release(cs_code);

    if (!pso)
    {
        ID3D12RootSignature_Release(root_signature);
        return false;
    }

    /* With dynamic resources descriptor heaps must be set before the root signature. */
    ID3D12GraphicsCommandList_SetDescriptorHeaps(command_list, 1, &runner->heap);
    ID3D12GraphicsCommandList_SetComputeRootSignature(command_list, root_signature);
    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(command_list,
                uniform_index++, u->count, u->elements, 0);
    }

    if (r->descriptor_count)
        ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(command_list, 0,
                get_gpu_descriptor_handle(test_context, runner->heap, 0));

    ID3D12GraphicsCommandList_SetPipelineState(command_list, pso);
    ID3D12GraphicsCommandList_Dispatch(command_list, x, y, z);
    ID3D12RootSignature_Release(root_signature);

    /* Finish the command list so that we can destroy objects.
     * Also, subsequent UAV probes will use the graphics command list, so make
     * sure that the above barriers are actually executed. */
    hr = ID3D12GraphicsCommandList_Close(command_list);
    ok(hr == S_OK, "Failed to close command list, hr %#x.\n", hr);
    exec_command_list(queue, command_list);
    wait_queue_idle(device, queue);
    reset_command_list(command_list, allocator);

    ID3D12PipelineState_Release(pso);

    return true;
}

static void d3d12_runner_clear(struct shader_runner *r, struct resource *resource, const struct vec4 *clear_value)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *test_context = &runner->test_context;

    ID3D12GraphicsCommandList *command_list = test_context->list;
    ID3D12CommandQueue *queue = test_context->queue;
    ID3D12Device *device = test_context->device;
    D3D12_CPU_DESCRIPTOR_HANDLE view;
    HRESULT hr;

    switch (resource->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            view = get_cpu_rtv_handle(test_context, runner->rtv_heap, resource->desc.slot);
            ID3D12GraphicsCommandList_ClearRenderTargetView(command_list, view, (const float *)clear_value, 0, NULL);
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            view = get_cpu_dsv_handle(test_context, runner->dsv_heap, 0);
            ID3D12GraphicsCommandList_ClearDepthStencilView(command_list, view,
                    D3D12_CLEAR_FLAG_DEPTH, clear_value->x, 0, 0, NULL);
            break;

        default:
            fatal_error("Clears are not implemented for resource type %u.\n", resource->desc.type);
    }

    hr = ID3D12GraphicsCommandList_Close(command_list);
    ok(hr == S_OK, "Failed to close command list, hr %#x.\n", hr);
    exec_command_list(queue, command_list);
    wait_queue_idle(device, queue);
    reset_command_list(command_list, test_context->allocator);
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE d3d12_primitive_topology_type_from_primitive_topology(
        D3D_PRIMITIVE_TOPOLOGY primitive_topology)
{
    switch (primitive_topology)
    {
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
        case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
        case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
        case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            if (primitive_topology >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
                    && primitive_topology <= D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST)
                return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            fatal_error("Unhandled primitive topology %u.\n", primitive_topology);
    }
}

static D3D12_INPUT_ELEMENT_DESC *create_element_descs(const struct d3d12_shader_runner *runner)
{
    D3D12_INPUT_ELEMENT_DESC *input_element_descs = calloc(runner->r.input.element_count, sizeof(*input_element_descs));
    for (size_t i = 0; i < runner->r.input.element_count; ++i)
    {
        const struct input_element *element = &runner->r.input.elements[i];
        D3D12_INPUT_ELEMENT_DESC *desc = &input_element_descs[i];

        desc->SemanticName = element->name;
        desc->SemanticIndex = element->index;
        desc->Format = element->format;
        desc->InputSlot = element->slot;
        desc->AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        desc->InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    }

    return input_element_descs;
}

static ID3D12PipelineState *create_pipeline(struct d3d12_shader_runner *runner,
        D3D12_PRIMITIVE_TOPOLOGY primitive_topology, ID3D10Blob *vs_code, ID3D10Blob *ps_code,
        ID3D10Blob *hs_code, ID3D10Blob *ds_code, ID3D10Blob *gs_code)
{
    struct test_context *test_context = &runner->test_context;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {0};
    D3D12_INPUT_ELEMENT_DESC *input_element_descs;
    ID3D12Device *device = test_context->device;
    unsigned int sample_count = 1;
    ID3D12PipelineState *pso;
    HRESULT hr;

    for (size_t i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d12_resource *resource = d3d12_resource(runner->r.resources[i]);

        if (resource->r.desc.type == RESOURCE_TYPE_RENDER_TARGET)
        {
            pso_desc.RTVFormats[resource->r.desc.slot] = resource->r.desc.format;
            pso_desc.NumRenderTargets = max(pso_desc.NumRenderTargets, resource->r.desc.slot + 1);
            pso_desc.BlendState.RenderTarget[resource->r.desc.slot].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            if (resource->r.desc.sample_count)
                sample_count = resource->r.desc.sample_count;
        }
        else if (resource->r.desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        {
            assert(!resource->r.desc.slot);
            pso_desc.DSVFormat = resource->r.desc.format;
            pso_desc.DepthStencilState.DepthEnable = true;
            pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            pso_desc.DepthStencilState.DepthFunc = runner->r.depth_func;
        }
    }

    pso_desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs_code);
    pso_desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs_code);
    pso_desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps_code);
    pso_desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps_code);
    if (hs_code)
    {
        pso_desc.HS.pShaderBytecode = ID3D10Blob_GetBufferPointer(hs_code);
        pso_desc.HS.BytecodeLength = ID3D10Blob_GetBufferSize(hs_code);
    }
    if (ds_code)
    {
        pso_desc.DS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ds_code);
        pso_desc.DS.BytecodeLength = ID3D10Blob_GetBufferSize(ds_code);
    }
    if (gs_code)
    {
        pso_desc.GS.pShaderBytecode = ID3D10Blob_GetBufferPointer(gs_code);
        pso_desc.GS.BytecodeLength = ID3D10Blob_GetBufferSize(gs_code);
    }
    pso_desc.PrimitiveTopologyType = d3d12_primitive_topology_type_from_primitive_topology(primitive_topology);
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.SampleDesc.Count = sample_count;
    pso_desc.SampleMask = runner->r.sample_mask ? runner->r.sample_mask : ~(UINT)0;
    pso_desc.pRootSignature = test_context->root_signature;

    input_element_descs = create_element_descs(runner);
    pso_desc.InputLayout.pInputElementDescs = input_element_descs;
    pso_desc.InputLayout.NumElements = runner->r.input.element_count;

    hr = ID3D12Device_CreateGraphicsPipelineState(device, &pso_desc,
            &IID_ID3D12PipelineState, (void **)&pso);
    todo_if(runner->r.is_todo) bug_if(runner->r.is_bug)
    ok(hr == S_OK, "Failed to create state, hr %#x.\n", hr);

    free(input_element_descs);

    if (FAILED(hr))
        return NULL;

    return pso;
}

static ID3D12PipelineState *create_pipeline_device2(struct d3d12_shader_runner *runner,
        D3D12_PRIMITIVE_TOPOLOGY primitive_topology, ID3D10Blob *vs_code, ID3D10Blob *ps_code,
        ID3D10Blob *hs_code, ID3D10Blob *ds_code, ID3D10Blob *gs_code)
{
    struct test_context *test_context = &runner->test_context;
    ID3D12Device2 *device2 = test_context->device2;
    D3D12_PIPELINE_STATE_STREAM_DESC pipeline_desc;
    D3D12_INPUT_ELEMENT_DESC *input_element_descs;
    unsigned int sample_count = 1;
    ID3D12PipelineState *pso;
    HRESULT hr;

    struct
    {
        struct d3d12_root_signature_subobject root_signature;
        struct d3d12_shader_bytecode_subobject vs;
        struct d3d12_shader_bytecode_subobject ps;
        struct d3d12_shader_bytecode_subobject hs;
        struct d3d12_shader_bytecode_subobject ds;
        struct d3d12_shader_bytecode_subobject gs;
        struct d3d12_render_target_formats_subobject rtv_format;
        struct d3d12_blend_subobject blend;
        struct d3d12_depth_stencil_format_subobject dsv_format;
        struct d3d12_depth_stencil1_subobject dsv;
        struct d3d12_rasterizer_subobject rasterizer;
        struct d3d12_primitive_topology_subobject topology;
        struct d3d12_sample_desc_subobject sample_desc;
        struct d3d12_sample_mask_subobject sample_mask;
        struct d3d12_input_layout_subobject input_layout;
    }
    pipeline =
    {
        .root_signature =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE,
            .root_signature = test_context->root_signature,
        },
        .vs =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS,
            .shader_bytecode = shader_bytecode_from_blob(vs_code),
        },
        .ps =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS,
            .shader_bytecode = shader_bytecode_from_blob(ps_code),
        },
        .hs =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS,
            .shader_bytecode = hs_code ? shader_bytecode_from_blob(hs_code) : (D3D12_SHADER_BYTECODE) {0},
        },
        .ds =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS,
            .shader_bytecode = ds_code ? shader_bytecode_from_blob(ds_code) : (D3D12_SHADER_BYTECODE) {0},
        },
        .gs =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS,
            .shader_bytecode = gs_code ? shader_bytecode_from_blob(gs_code) : (D3D12_SHADER_BYTECODE) {0},
        },
        .rtv_format =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS,
        },
        .blend =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND,
        },
        .dsv_format =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT,
        },
        .dsv =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1,
        },
        .rasterizer =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER,
            .rasterizer_desc =
        {
                .FillMode = D3D12_FILL_MODE_SOLID,
                .CullMode = D3D12_CULL_MODE_NONE,
            }
        },
        .topology =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY,
            .primitive_topology_type = d3d12_primitive_topology_type_from_primitive_topology(primitive_topology),
        },
        .sample_desc =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC,
        },
        .sample_mask =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK,
            .sample_mask = runner->r.sample_mask ? runner->r.sample_mask : ~(UINT)0,
        },
        .input_layout =
        {
            .type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT,
        },
    };

    if (!device2)
        return NULL;

    input_element_descs = create_element_descs(runner);
    pipeline.input_layout.input_layout.NumElements = runner->r.input.element_count;
    pipeline.input_layout.input_layout.pInputElementDescs = input_element_descs;

    for (size_t i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d12_resource *resource = d3d12_resource(runner->r.resources[i]);

        if (resource->r.desc.type == RESOURCE_TYPE_RENDER_TARGET)
        {
            pipeline.rtv_format.render_target_formats.RTFormats[resource->r.desc.slot] = resource->r.desc.format;
            pipeline.rtv_format.render_target_formats.NumRenderTargets =
                    max(pipeline.rtv_format.render_target_formats.NumRenderTargets, resource->r.desc.slot + 1);
            pipeline.blend.blend_desc.RenderTarget[resource->r.desc.slot].RenderTargetWriteMask =
                    D3D12_COLOR_WRITE_ENABLE_ALL;
            if (resource->r.desc.sample_count)
                sample_count = resource->r.desc.sample_count;
        }
        else if (resource->r.desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        {
            assert(!resource->r.desc.slot);
            pipeline.dsv_format.depth_stencil_format = resource->r.desc.format;
            pipeline.dsv.depth_stencil_desc.DepthEnable = true;
            pipeline.dsv.depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            pipeline.dsv.depth_stencil_desc.DepthFunc = runner->r.depth_func;
            pipeline.dsv.depth_stencil_desc.DepthBoundsTestEnable = runner->r.depth_bounds;
        }
    }

    pipeline.sample_desc.sample_desc.Count = sample_count;

    pipeline_desc.SizeInBytes = sizeof(pipeline);
    pipeline_desc.pPipelineStateSubobjectStream = &pipeline;

    hr = ID3D12Device2_CreatePipelineState(device2, &pipeline_desc, &IID_ID3D12PipelineState, (void **)&pso);
    todo_if(runner->r.is_todo) bug_if(runner->r.is_bug)
    ok(hr == S_OK, "Failed to create state, hr %#x.\n", hr);

    free(input_element_descs);

    if (FAILED(hr))
        return NULL;
    return pso;
}

static bool d3d12_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY primitive_topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *test_context = &runner->test_context;

    ID3D10Blob *vs_code, *ps_code, *hs_code = NULL, *ds_code = NULL, *gs_code = NULL;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
    unsigned int uniform_index, fb_width, fb_height, rtv_count = 0, viewport_count;
    ID3D12GraphicsCommandList1 *command_list1 = test_context->list1;
    ID3D12GraphicsCommandList *command_list = test_context->list;
    D3D12_VIEWPORT viewports[ARRAY_SIZE(r->viewports)];
    ID3D12CommandQueue *queue = test_context->queue;
    RECT scissor_rects[ARRAY_SIZE(r->viewports)];
    ID3D12Device *device = test_context->device;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {0};
    ID3D12PipelineState *pso;
    bool succeeded;
    HRESULT hr;
    size_t i;

    ps_code = compile_hlsl(&runner->r, SHADER_TYPE_PS);
    vs_code = compile_hlsl(&runner->r, SHADER_TYPE_VS);
    succeeded = ps_code && vs_code;

    if (runner->r.shader_source[SHADER_TYPE_HS])
    {
        hs_code = compile_hlsl(&runner->r, SHADER_TYPE_HS);
        succeeded = succeeded && hs_code;
    }
    if (runner->r.shader_source[SHADER_TYPE_DS])
    {
        ds_code = compile_hlsl(&runner->r, SHADER_TYPE_DS);
        succeeded = succeeded && ds_code;
    }
    if (runner->r.shader_source[SHADER_TYPE_GS])
    {
        gs_code = compile_hlsl(&runner->r, SHADER_TYPE_GS);
        succeeded = succeeded && gs_code;
    }

    if (!succeeded)
    {
        todo_if(runner->r.is_todo && runner->r.shader_model < SHADER_MODEL_6_0)
        ok(false, "Failed to compile shaders.\n");

        if (ps_code)
            ID3D10Blob_Release(ps_code);
        if (vs_code)
            ID3D10Blob_Release(vs_code);
        if (hs_code)
            ID3D10Blob_Release(hs_code);
        if (ds_code)
            ID3D10Blob_Release(ds_code);
        if (gs_code)
            ID3D10Blob_Release(gs_code);
        return false;
    }

    test_context->root_signature = d3d12_runner_create_root_signature(runner,
            queue, test_context->allocator, command_list, &uniform_index);

    write_descriptors(runner);

    pso = test_context->device2
            ?  create_pipeline_device2(runner, primitive_topology, vs_code, ps_code, hs_code, ds_code, gs_code)
            : create_pipeline(runner, primitive_topology, vs_code, ps_code, hs_code, ds_code, gs_code);

    ID3D10Blob_Release(vs_code);
    ID3D10Blob_Release(ps_code);
    if (hs_code)
        ID3D10Blob_Release(hs_code);
    if (ds_code)
        ID3D10Blob_Release(ds_code);
    if (gs_code)
        ID3D10Blob_Release(gs_code);

    if (!pso)
    {
        ID3D12RootSignature_Release(test_context->root_signature);
        test_context->root_signature = NULL;
        return false;
    }

    fb_width = ~0u;
    fb_height = ~0u;

    /* With dynamic resources descriptor heaps must be set before the root signature. */
    ID3D12GraphicsCommandList_SetDescriptorHeaps(command_list, 1, &runner->heap);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(command_list, test_context->root_signature);

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(command_list,
                uniform_index++, u->count, u->elements, 0);
    }

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d12_resource *resource = d3d12_resource(runner->r.resources[i]);
        D3D12_VERTEX_BUFFER_VIEW vbv;

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
                rtvs[resource->r.desc.slot] = get_cpu_rtv_handle(test_context, runner->rtv_heap, resource->r.desc.slot);
                rtv_count = max(rtv_count, resource->r.desc.slot + 1);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_DEPTH_STENCIL:
                dsv = get_cpu_dsv_handle(test_context, runner->dsv_heap, 0);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_VERTEX_BUFFER:
                vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(resource->resource);
                vbv.StrideInBytes = get_vb_stride(&runner->r, resource->r.desc.slot);
                vbv.SizeInBytes = resource->r.desc.width;

                ID3D12GraphicsCommandList_IASetVertexBuffers(command_list, resource->r.desc.slot, 1, &vbv);
                break;

            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UAV:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }

    if (r->descriptor_count)
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(command_list, 0,
                get_gpu_descriptor_handle(test_context, runner->heap, 0));

    set_rect(&test_context->scissor_rect, 0, 0, fb_width, fb_height);
    set_viewport(&test_context->viewport, 0.0f, 0.0f, fb_width, fb_height, 0.0f, 1.0f);
    viewports[0] = test_context->viewport;
    scissor_rects[0] = test_context->scissor_rect;
    viewport_count = max(r->viewport_count, 1);
    for (i = 0; i < r->viewport_count; ++i)
    {
        viewports[i].TopLeftX = r->viewports[i].x;
        viewports[i].TopLeftY = r->viewports[i].y;
        viewports[i].Width = r->viewports[i].width;
        viewports[i].Height = r->viewports[i].height;
        viewports[i].MinDepth = 0.0f;
        viewports[i].MaxDepth = 1.0f;
        scissor_rects[i] = test_context->scissor_rect;
    }

    ID3D12GraphicsCommandList_OMSetRenderTargets(command_list, rtv_count, rtvs, false, dsv.ptr ? &dsv : NULL);

    if (runner->r.depth_bounds)
        ID3D12GraphicsCommandList1_OMSetDepthBounds(command_list1, runner->r.depth_min, runner->r.depth_max);
    ID3D12GraphicsCommandList_RSSetScissorRects(command_list, viewport_count, scissor_rects);
    ID3D12GraphicsCommandList_RSSetViewports(command_list, viewport_count, viewports);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(command_list, primitive_topology);
    ID3D12GraphicsCommandList_SetPipelineState(command_list, pso);
    ID3D12GraphicsCommandList_DrawInstanced(command_list, vertex_count, instance_count, first_vertex, first_instance);

    /* Finish the command list so that we can destroy objects. */
    hr = ID3D12GraphicsCommandList_Close(command_list);
    ok(hr == S_OK, "Failed to close command list, hr %#x.\n", hr);
    exec_command_list(queue, command_list);
    wait_queue_idle(device, queue);
    reset_command_list(command_list, test_context->allocator);

    if (test_context->root_signature)
        ID3D12RootSignature_Release(test_context->root_signature);
    test_context->root_signature = NULL;

    ID3D12PipelineState_Release(pso);

    return true;
}

static bool d3d12_runner_copy(struct shader_runner *r, struct resource *src, struct resource *dst)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *context = &runner->test_context;
    struct d3d12_resource *s = d3d12_resource(src);
    struct d3d12_resource *d = d3d12_resource(dst);
    D3D12_RESOURCE_STATES src_state, dst_state;
    HRESULT hr;

    src_state = resource_get_state(src);
    dst_state = resource_get_state(dst);

    transition_resource_state(context->list, s->resource, src_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition_resource_state(context->list, d->resource, dst_state, D3D12_RESOURCE_STATE_COPY_DEST);
    ID3D12GraphicsCommandList_CopyResource(context->list, d->resource, s->resource);
    transition_resource_state(context->list, d->resource, D3D12_RESOURCE_STATE_COPY_DEST, dst_state);
    transition_resource_state(context->list, s->resource, D3D12_RESOURCE_STATE_COPY_SOURCE, src_state);

    hr = ID3D12GraphicsCommandList_Close(context->list);
    ok(hr == S_OK, "Failed to close command list, hr %#x.\n", hr);
    exec_command_list(context->queue, context->list);
    wait_queue_idle(context->device, context->queue);
    reset_command_list(context->list, context->allocator);

    return true;
}

static struct resource_readback *d3d12_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct d3d12_shader_runner *runner = d3d12_shader_runner(r);
    struct test_context *test_context = &runner->test_context;
    struct d3d12_resource_readback *rb = malloc(sizeof(*rb));
    struct d3d12_resource *resource = d3d12_resource(res);
    D3D12_RESOURCE_STATES state;

    state = resource_get_state(res);
    get_resource_readback_with_command_list_and_states(resource->resource,
            sub_resource_idx, rb, test_context->queue, test_context->list, state, state);
    reset_command_list(test_context->list, test_context->allocator);

    return &rb->rb;
}

static void d3d12_runner_release_readback(struct shader_runner *r, struct resource_readback *rb)
{
    struct d3d12_resource_readback *d3d12_rb = CONTAINING_RECORD(rb, struct d3d12_resource_readback, rb);

    release_resource_readback(d3d12_rb);
    free(d3d12_rb);
}

static const struct shader_runner_ops d3d12_runner_ops =
{
    .create_resource = d3d12_runner_create_resource,
    .destroy_resource = d3d12_runner_destroy_resource,
    .dispatch = d3d12_runner_dispatch,
    .clear = d3d12_runner_clear,
    .draw = d3d12_runner_draw,
    .copy = d3d12_runner_copy,
    .get_resource_readback = d3d12_runner_get_resource_readback,
    .release_readback = d3d12_runner_release_readback,
};

static uint32_t get_format_support(ID3D12Device *device, enum DXGI_FORMAT format)
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = {.Format = format};
    uint32_t ret = 0;
    HRESULT hr;

    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_FORMAT_SUPPORT,
            &format_support, sizeof(format_support));
    ok(hr == S_OK, "Failed to query format support, hr %#x.\n", hr);

    if (format_support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)
        ret |= FORMAT_CAP_UAV_LOAD;

    return ret;
}

static void d3d12_runner_init_caps(struct d3d12_shader_runner *runner,
        enum shader_model minimum_shader_model, enum shader_model maximum_shader_model,
        bool using_dxcompiler)
{
    ID3D12Device *device = runner->test_context.device;
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4;
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 options2;
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1;
    D3D12_FEATURE_DATA_D3D12_OPTIONS options;
    HRESULT hr;

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
        DXGI_FORMAT_R32G32_FLOAT,
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

    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
    ok(hr == S_OK, "Failed to check feature options support, hr %#x.\n", hr);
    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1));
    ok(hr == S_OK, "Failed to check feature options1 support, hr %#x.\n", hr);
    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS2, &options2, sizeof(options2));
    ok(hr == S_OK, "Failed to check feature options2 support, hr %#x.\n", hr);
    hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4));
    ok(hr == S_OK, "Failed to check feature options4 support, hr %#x.\n", hr);

#ifdef VKD3D_CROSSTEST
    runner->caps.runner = "d3d12.dll";
#else
    runner->caps.runner = "vkd3d";
#endif
    runner->caps.compiler = using_dxcompiler ? "dxcompiler" : HLSL_COMPILER;
    runner->caps.minimum_shader_model = minimum_shader_model;
    runner->caps.maximum_shader_model = maximum_shader_model;
    if (is_cull_distance_supported(device))
        runner->caps.shader_caps[SHADER_CAP_CULL_DISTANCE] = true;
    runner->caps.shader_caps[SHADER_CAP_DEPTH_BOUNDS] = options2.DepthBoundsTestSupported;
    runner->caps.shader_caps[SHADER_CAP_DESCRIPTORS] = true;
    runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX] = true;
    runner->caps.shader_caps[SHADER_CAP_FLOAT64] = options.DoublePrecisionFloatShaderOps;
    if (is_geometry_shader_supported(device))
        runner->caps.shader_caps[SHADER_CAP_GEOMETRY_SHADER] = true;
    runner->caps.shader_caps[SHADER_CAP_INT64] = options1.Int64ShaderOps;
    runner->caps.shader_caps[SHADER_CAP_MIN_LOD_CLAMP] = true;
    runner->caps.shader_caps[SHADER_CAP_ROV] = options.ROVsSupported;
    runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX]
            = options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation;
    runner->caps.shader_caps[SHADER_CAP_TESSELLATION_SHADER] = true;
    runner->caps.shader_caps[SHADER_CAP_WAVE_OPS] = options1.WaveOps;
    runner->caps.shader_caps[SHADER_CAP_NATIVE_16_BIT] = options4.Native16BitShaderOpsSupported;

    runner->caps.tags[SHADER_RUNNER_TAG_D3D12].present = true;

    runner->caps.tags[SHADER_RUNNER_TAG_MVK].present = get_mvk_device_version(device,
            &runner->caps.tags[SHADER_RUNNER_TAG_MVK].major, &runner->caps.tags[SHADER_RUNNER_TAG_MVK].minor,
            &runner->caps.tags[SHADER_RUNNER_TAG_MVK].patch);

    if (is_llvmpipe_device(device))
        runner->caps.tags[SHADER_RUNNER_TAG_LLVMPIPE].present = true;

    if (is_radv_device(device))
        runner->caps.tags[SHADER_RUNNER_TAG_RADV].present = true;

    if (is_intel_device(device))
        runner->caps.tags[SHADER_RUNNER_TAG_INTEL].present = true;

    runner->caps.tags[SHADER_RUNNER_TAG_MESA].present = get_mesa_device_version(device,
            &runner->caps.tags[SHADER_RUNNER_TAG_MESA].major, &runner->caps.tags[SHADER_RUNNER_TAG_MESA].minor,
            &runner->caps.tags[SHADER_RUNNER_TAG_MESA].patch);

    runner->caps.tags[SHADER_RUNNER_TAG_LLVM].present = get_llvm_device_version(device,
            &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].major, &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].minor,
            &runner->caps.tags[SHADER_RUNNER_TAG_LLVM].patch);

    if (test_options.use_warp_device)
        runner->caps.tags[SHADER_RUNNER_TAG_WARP].present = true;

    if (is_amd_windows_device(device) || is_radv_device(device))
        runner->caps.tags[SHADER_RUNNER_TAG_AMD].present = true;

    if (is_nvidia_windows_device(device) || is_nvidia_device(device))
        runner->caps.tags[SHADER_RUNNER_TAG_NVIDIA].present = true;

#ifdef _WIN32
    runner->caps.tags[SHADER_RUNNER_TAG_WINDOWS].present = true;
#endif

    for (unsigned int i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        runner->caps.format_caps[formats[i]] = get_format_support(device, formats[i]);
    }

    runner->heap = create_gpu_descriptor_heap(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_RESOURCE_DESCRIPTORS);
}

static enum shader_model get_supported_shader_model(ID3D12Device *device)
{
    D3D12_FEATURE_DATA_SHADER_MODEL sm_feature;
    D3D_SHADER_MODEL sm;
    unsigned int i;
    HRESULT hr;

    static const D3D_SHADER_MODEL d3d_shader_models[] =
    {
        [SHADER_MODEL_5_1] = D3D_SHADER_MODEL_5_1,
        [SHADER_MODEL_6_0] = D3D_SHADER_MODEL_6_0,
        [SHADER_MODEL_6_2] = D3D_SHADER_MODEL_6_2,
        [SHADER_MODEL_6_6] = D3D_SHADER_MODEL_6_6,
    };

    for (i = ARRAY_SIZE(d3d_shader_models) - 1; i != UINT_MAX; --i)
    {
        if (!(sm = d3d_shader_models[i]))
            continue;

        sm_feature.HighestShaderModel = sm;

        hr = ID3D12Device_CheckFeatureSupport(device, D3D12_FEATURE_SHADER_MODEL, &sm_feature, sizeof(sm_feature));
        /* Might return E_INVALIDARG is the runtime doesn't know about modern SMs. */
        ok(hr == S_OK || hr == E_INVALIDARG, "Failed to check feature shader model support, hr %#x.\n", hr);

        if (hr == S_OK && sm_feature.HighestShaderModel >= sm)
            return i;
    }

    fatal_error("Maximum shader model is smaller than 5.1.\n");
}

static void run_shader_tests_for_model_range(void *dxc_compiler,
        enum shader_model minimum_shader_model, enum shader_model maximum_shader_model)
{
    static const struct test_context_desc desc =
    {
        .rt_width = RENDER_TARGET_WIDTH,
        .rt_height = RENDER_TARGET_HEIGHT,
        .no_root_signature = true,
        .rt_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
    };
    struct d3d12_shader_runner runner = {0};
    ID3D12Device *device;
    HRESULT hr;

    if (!init_test_context(&runner.test_context, &desc))
        return;
    device = runner.test_context.device;

    maximum_shader_model = min(maximum_shader_model, get_supported_shader_model(device));

    if (minimum_shader_model >= SHADER_MODEL_6_0 && maximum_shader_model < SHADER_MODEL_6_0)
    {
        skip("The device does not support shader model 6.0.\n");
        destroy_test_context(&runner.test_context);
        return;
    }

    d3d12_runner_init_caps(&runner, minimum_shader_model, maximum_shader_model, !!dxc_compiler);

    runner.compute_queue = create_command_queue(device,
            D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);

    hr = ID3D12Device_CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_COMPUTE,
            &IID_ID3D12CommandAllocator, (void **)&runner.compute_allocator);
    ok(hr == S_OK, "Failed to create command allocator, hr %#x.\n", hr);

    hr = ID3D12Device_CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
            runner.compute_allocator, NULL, &IID_ID3D12GraphicsCommandList, (void **)&runner.compute_list);
    ok(hr == S_OK, "Failed to create command list, hr %#x.\n", hr);

    run_shader_tests(&runner.r, &runner.caps, &d3d12_runner_ops, dxc_compiler);

    ID3D12GraphicsCommandList_Release(runner.compute_list);
    ID3D12CommandAllocator_Release(runner.compute_allocator);
    ID3D12CommandQueue_Release(runner.compute_queue);
    if (runner.heap)
        ID3D12DescriptorHeap_Release(runner.heap);
    if (runner.rtv_heap)
        ID3D12DescriptorHeap_Release(runner.rtv_heap);
    if (runner.dsv_heap)
        ID3D12DescriptorHeap_Release(runner.dsv_heap);
    destroy_test_context(&runner.test_context);
}

void run_shader_tests_d3d12(void *dxc_compiler)
{
#ifdef VKD3D_CROSSTEST
    const char *executor = "d3d12.dll";
#else
    const char *executor = "vkd3d";
#endif

    bool skip_sm4 = test_skipping_execution(executor, HLSL_COMPILER, SHADER_MODEL_4_0, SHADER_MODEL_5_1);
    bool skip_sm6 = test_skipping_execution(executor, "dxcompiler", SHADER_MODEL_6_0, SHADER_MODEL_MAX);

    if (skip_sm4 && skip_sm6)
        return;

    enable_d3d12_debug_layer();
    init_adapter_info();

    if (!skip_sm4)
        run_shader_tests_for_model_range(NULL, SHADER_MODEL_4_0, SHADER_MODEL_5_1);

    if (dxc_compiler && !skip_sm6)
        run_shader_tests_for_model_range(dxc_compiler, SHADER_MODEL_6_0, SHADER_MODEL_MAX);
}
