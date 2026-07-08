/*
 * Copyright 2025 Henri Verbeet
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

#define INITGUID
#define _GNU_SOURCE
#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include "demo.h"

#include "teapot.h"
#include "etl16-unicode.h"

DEMO_EMBED(teapot_hlsl, "teapot.hlsl");
DEMO_EMBED(text_hlsl, "text.hlsl");

struct teapot_fence
{
    ID3D12Fence *fence;
    UINT64 value;
};

struct teapot_cb_data
{
    struct demo_matrix mvp_matrix;
    struct demo_vec3 eye;
    float level;
    unsigned int wireframe, flat;
};

struct demo_text_run
{
    struct demo_vec4 colour;
    struct demo_uvec2 position;
    unsigned int start_idx;         /* The start offset of the run in the "text_buffer" buffer. */
    unsigned int char_count;
    unsigned int reverse;
    float scale;
};

struct demo_text_cb_data
{
    struct demo_uvec4 screen_size;
    struct demo_uvec4 glyphs[96];
};

struct demo_text
{
    ID3D12Device *device;

    ID3D12RootSignature *root_signature;
    ID3D12CommandSignature *command_signature;
    ID3D12PipelineState *pipeline_state;
    ID3D12DescriptorHeap *srv_heap;
    ID3D12Resource *argument_buffer, *text_cb, *text_buffer, *vb;
    D3D12_VERTEX_BUFFER_VIEW vbv;

    unsigned int screen_width, screen_height;
    D3D12_DRAW_ARGUMENTS *draw_arguments;
    struct demo_text_run *runs;
    size_t run_count, runs_size;
    char *text;
    size_t char_count, text_size;

    float scale;
    bool reverse;
};

struct teapot
{
    struct demo demo;

    struct demo_window *window;

    unsigned int width, height;
    unsigned int tessellation_level;
    unsigned int text_scale;
    float theta, phi;
    float theta_dir;

    bool animate, display_help, flat, wireframe;
    struct timeval last_text;
    struct timeval frame_times[16];
    size_t frame_count;
    double t_animate;

    D3D12_VIEWPORT vp;
    D3D12_RECT scissor_rect;

    ID3D12Device *device;
    ID3D12CommandQueue *command_queue;
    struct demo_swapchain *swapchain;
    struct
    {
        ID3D12Resource *render_target;
        ID3D12CommandAllocator *command_allocator;
        ID3D12GraphicsCommandList *command_list;
    } *swapchain_images;
    ID3D12DescriptorHeap *rtv_heap, *dsv_heap;
    unsigned int rtv_descriptor_size;

    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline_state;
    ID3D12Resource *ds, *cb, *vb, *ib;
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW ibv;

    unsigned int rt_idx;
    struct teapot_fence fence;

    struct teapot_cb_data *cb_data;

    struct demo_text text;
};

static double timeval_diff(const struct timeval *end, const struct timeval *start)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}

static ID3D12Resource *create_buffer(ID3D12Device *device, size_t size)
{
    D3D12_RESOURCE_DESC resource_desc;
    D3D12_HEAP_PROPERTIES heap_desc;
    ID3D12Resource *buffer;
    HRESULT hr;

    heap_desc.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_desc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_desc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_desc.CreationNodeMask = 1;
    heap_desc.VisibleNodeMask = 1;

    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    hr = ID3D12Device_CreateCommittedResource(device, &heap_desc, D3D12_HEAP_FLAG_NONE, &resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void **)&buffer);
    assert(SUCCEEDED(hr));

    return buffer;
}

static void demo_text_populate_command_list(struct demo_text *text, ID3D12GraphicsCommandList *command_list)
{
    ID3D12GraphicsCommandList_SetPipelineState(command_list, text->pipeline_state);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(command_list, text->root_signature);
    ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(command_list, 0,
            ID3D12Resource_GetGPUVirtualAddress(text->text_cb));
    ID3D12GraphicsCommandList_SetDescriptorHeaps(command_list, 1, &text->srv_heap);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(command_list, 1,
            ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(text->srv_heap));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(command_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D12GraphicsCommandList_IASetVertexBuffers(command_list, 0, 1, &text->vbv);

    ID3D12GraphicsCommandList_ExecuteIndirect(command_list,
            text->command_signature, 1, text->argument_buffer, 0, NULL, 0);
}

static void DEMO_PRINTF_FUNC(5, 6) demo_text_draw(struct demo_text *text,
        const struct demo_vec4 *colour, int x, int y, const char *format, ...)
{
    struct demo_text_run *t;
    va_list args;
    size_t rem;
    int rc;

    for (;;)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
        ID3D12Resource *text_buffer;
        size_t text_size;
        HRESULT hr;
        char *p;

        rem = text->text_size - text->char_count;
        va_start(args, format);
        rc = vsnprintf(&text->text[text->char_count], rem, format, args);
        va_end(args);
        if (rc >= 0 && (unsigned int)rc < rem)
            break;

        text_size = text->text_size * 2;
        if (rc >= 0)
            while (text_size < text->char_count + rc + 1)
                text_size *= 2;

        text_buffer = create_buffer(text->device, text_size);
        hr = ID3D12Resource_Map(text_buffer, 0, &(D3D12_RANGE){0, 0}, (void **)&p);
        assert(SUCCEEDED(hr));
        memcpy(p, text->text, text->char_count);
        ID3D12Resource_Unmap(text->text_buffer, 0, NULL);
        ID3D12Resource_Release(text->text_buffer);
        text->text_size = text_size;
        text->text = p;
        text->text_buffer = text_buffer;

        srv_desc.Format = DXGI_FORMAT_R8_UINT;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = text->text_size;
        srv_desc.Buffer.StructureByteStride = 0;
        srv_desc.Buffer.Flags = 0;
        ID3D12Device_CreateShaderResourceView(text->device, text_buffer, &srv_desc,
                ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(text->srv_heap));
    }

    if (text->run_count == text->runs_size)
    {
        struct demo_text_run *runs;
        ID3D12Resource *vb;
        size_t runs_size;
        HRESULT hr;

        runs_size = text->runs_size * 2;
        vb = create_buffer(text->device, runs_size * sizeof(*runs));
        hr = ID3D12Resource_Map(vb, 0, &(D3D12_RANGE){0, 0}, (void **)&runs);
        assert(SUCCEEDED(hr));
        memcpy(runs, text->runs, text->run_count * sizeof(*text->runs));
        ID3D12Resource_Unmap(text->vb, 0, NULL);
        ID3D12Resource_Release(text->vb);
        text->runs_size = runs_size;
        text->runs = runs;
        text->vb = vb;

        text->vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(vb);
        text->vbv.SizeInBytes = runs_size * sizeof(*runs);
    }

    t = &text->runs[text->run_count++];
    t->colour = *colour;
    t->position.x = x < 0 ? text->screen_width + x : x;
    t->position.y = y < 0 ? text->screen_height + y : y;
    t->start_idx = text->char_count;
    t->char_count = rc;
    t->reverse = text->reverse;
    t->scale = text->scale;

    text->char_count += rc;
}

static void demo_text_init(struct demo_text *text, ID3D12Device *device,
        unsigned int screen_width, unsigned int screen_height, unsigned int scale)
{
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
    D3D12_COMMAND_SIGNATURE_DESC signature_desc;
    D3D12_INDIRECT_ARGUMENT_DESC argument_desc;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc;
    D3D12_DESCRIPTOR_RANGE descriptor_range;
    D3D12_ROOT_PARAMETER root_parameters[2];
    struct demo_text_cb_data *text_cb_data;
    ID3DBlob *vs, *ps;
    HRESULT hr;

    static const D3D12_INPUT_ELEMENT_DESC il_desc[] =
    {
        {"COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"POSITION", 0, DXGI_FORMAT_R32G32_UINT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"IDX", 0, DXGI_FORMAT_R32_UINT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"COUNT", 0, DXGI_FORMAT_R32_UINT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"REVERSE", 0, DXGI_FORMAT_R32_UINT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"SCALE", 0, DXGI_FORMAT_R32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };

    text->device = device;
    ID3D12Device_AddRef(device);

    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_parameters[0].Descriptor.ShaderRegister = 0;
    root_parameters[0].Descriptor.RegisterSpace = 0;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptor_range.NumDescriptors = 1;
    descriptor_range.BaseShaderRegister = 0;
    descriptor_range.RegisterSpace = 0;
    descriptor_range.OffsetInDescriptorsFromTableStart = 0;
    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[1].DescriptorTable.pDescriptorRanges = &descriptor_range;
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    memset(&root_signature_desc, 0, sizeof(root_signature_desc));
    root_signature_desc.NumParameters = ARRAY_SIZE(root_parameters);
    root_signature_desc.pParameters = root_parameters;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    hr = demo_create_root_signature(device, &root_signature_desc, &text->root_signature);
    assert(SUCCEEDED(hr));

    argument_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    signature_desc.NumArgumentDescs = 1;
    signature_desc.pArgumentDescs = &argument_desc;
    signature_desc.NodeMask = 0;

    hr = ID3D12Device_CreateCommandSignature(text->device, &signature_desc,
            NULL, &IID_ID3D12CommandSignature, (void **)&text->command_signature);
    assert(SUCCEEDED(hr));

    hr = D3DCompile(text_hlsl, text_hlsl_size, "text.hlsl", NULL, NULL, "vs_main", "vs_5_0", 0, 0, &vs, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DCompile(text_hlsl, text_hlsl_size, "text.hlsl", NULL, NULL, "ps_main", "ps_5_0", 0, 0, &ps, NULL);
    assert(SUCCEEDED(hr));

    memset(&pso_desc, 0, sizeof(pso_desc));
    pso_desc.InputLayout.pInputElementDescs = il_desc;
    pso_desc.InputLayout.NumElements = ARRAY_SIZE(il_desc);
    pso_desc.pRootSignature = text->root_signature;
    pso_desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
    pso_desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    pso_desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
    pso_desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);

    demo_rasterizer_desc_init_default(&pso_desc.RasterizerState);
    pso_desc.RasterizerState.FrontCounterClockwise = TRUE;
    demo_blend_desc_init_default(&pso_desc.BlendState);
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;

    hr = ID3D12Device_CreateGraphicsPipelineState(device, &pso_desc,
            &IID_ID3D12PipelineState, (void **)&text->pipeline_state);
    assert(SUCCEEDED(hr));

    ID3D10Blob_Release(ps);
    ID3D10Blob_Release(vs);

    memset(&srv_heap_desc, 0, sizeof(srv_heap_desc));
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = ID3D12Device_CreateDescriptorHeap(device, &srv_heap_desc,
            &IID_ID3D12DescriptorHeap, (void **)&text->srv_heap);
    assert(SUCCEEDED(hr));

    text->argument_buffer = create_buffer(device, sizeof(D3D12_DRAW_ARGUMENTS));
    hr = ID3D12Resource_Map(text->argument_buffer, 0, &(D3D12_RANGE){0, 0}, (void **)&text->draw_arguments);
    assert(SUCCEEDED(hr));

    text->text_cb = create_buffer(device, sizeof(*text_cb_data));
    hr = ID3D12Resource_Map(text->text_cb, 0, &(D3D12_RANGE){0, 0}, (void **)&text_cb_data);
    assert(SUCCEEDED(hr));

    text_cb_data->screen_size.x = screen_width;
    text_cb_data->screen_size.y = screen_height;
    text_cb_data->screen_size.z = scale;
    memcpy(text_cb_data->glyphs, etl16_unicode, sizeof(etl16_unicode));

    ID3D12Resource_Unmap(text->text_cb, 0, NULL);

    text->text_size = 4096;
    text->text_buffer = create_buffer(device, text->text_size);
    hr = ID3D12Resource_Map(text->text_buffer, 0, &(D3D12_RANGE){0, 0}, (void **)&text->text);
    assert(SUCCEEDED(hr));

    srv_desc.Format = DXGI_FORMAT_R8_UINT;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = text->text_size;
    srv_desc.Buffer.StructureByteStride = 0;
    srv_desc.Buffer.Flags = 0;
    ID3D12Device_CreateShaderResourceView(device, text->text_buffer, &srv_desc,
            ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(text->srv_heap));

    text->runs_size = 128;
    text->vb = create_buffer(device, text->runs_size * sizeof(*text->runs));
    hr = ID3D12Resource_Map(text->vb, 0, &(D3D12_RANGE){0, 0}, (void **)&text->runs);
    assert(SUCCEEDED(hr));

    text->vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(text->vb);
    text->vbv.StrideInBytes = sizeof(*text->runs);
    text->vbv.SizeInBytes = text->runs_size * sizeof(*text->runs);

    text->screen_width = screen_width;
    text->screen_height = screen_height;
}

static void demo_text_cleanup(struct demo_text *text)
{
    ID3D12Resource_Unmap(text->vb, 0, NULL);
    ID3D12Resource_Release(text->vb);
    ID3D12Resource_Unmap(text->text_buffer, 0, NULL);
    ID3D12Resource_Release(text->text_buffer);
    ID3D12Resource_Release(text->text_cb);
    ID3D12Resource_Unmap(text->argument_buffer, 0, NULL);
    ID3D12Resource_Release(text->argument_buffer);
    ID3D12DescriptorHeap_Release(text->srv_heap);
    ID3D12PipelineState_Release(text->pipeline_state);
    ID3D12CommandSignature_Release(text->command_signature);
    ID3D12RootSignature_Release(text->root_signature);
    ID3D12Device_Release(text->device);
}

static void teapot_populate_command_list(struct teapot *teapot,
        ID3D12GraphicsCommandList *command_list, unsigned int rt_idx)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle, dsv_handle;
    size_t rotate_idx_count, flip_idx_count;
    D3D12_RESOURCE_BARRIER barrier;
    HRESULT hr;

    hr = ID3D12GraphicsCommandList_Reset(command_list,
            teapot->swapchain_images[rt_idx].command_allocator, teapot->pipeline_state);
    assert(SUCCEEDED(hr));

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(command_list, teapot->root_signature);
    ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(command_list, 0,
            ID3D12Resource_GetGPUVirtualAddress(teapot->cb));

    ID3D12GraphicsCommandList_RSSetViewports(command_list, 1, &teapot->vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(command_list, 1, &teapot->scissor_rect);

    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = teapot->swapchain_images[rt_idx].render_target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(command_list, 1, &barrier);

    rtv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(teapot->rtv_heap);
    rtv_handle.ptr += rt_idx * teapot->rtv_descriptor_size;
    dsv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(teapot->dsv_heap);
    ID3D12GraphicsCommandList_OMSetRenderTargets(command_list, 1, &rtv_handle, FALSE, &dsv_handle);

    ID3D12GraphicsCommandList_ClearRenderTargetView(command_list, rtv_handle,
            (float[]){1.00f * 0.1f, 0.69f * 0.1f, 0.00f, 1.0f}, 0, NULL);
    ID3D12GraphicsCommandList_ClearDepthStencilView(command_list,
            dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(command_list, D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);
    ID3D12GraphicsCommandList_IASetIndexBuffer(command_list, &teapot->ibv);
    ID3D12GraphicsCommandList_IASetVertexBuffers(command_list, 0, 1, &teapot->vbv);

    rotate_idx_count = ARRAY_SIZE(teapot_rotate_patches) * 16;
    ID3D12GraphicsCommandList_DrawIndexedInstanced(command_list, rotate_idx_count, 4, 0, 0, 0);
    flip_idx_count = ARRAY_SIZE(teapot_flip_patches) * 16;
    ID3D12GraphicsCommandList_DrawIndexedInstanced(command_list, flip_idx_count, 2, rotate_idx_count, 0, 0);

    demo_text_populate_command_list(&teapot->text, command_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(command_list, 1, &barrier);

    hr = ID3D12GraphicsCommandList_Close(command_list);
    assert(SUCCEEDED(hr));
}

static void teapot_populate_command_lists(struct teapot *teapot)
{
    HRESULT hr;
    size_t i;

    for (i = 0; i < demo_swapchain_get_back_buffer_count(teapot->swapchain); ++i)
    {
        hr = ID3D12CommandAllocator_Reset(teapot->swapchain_images[i].command_allocator);
        assert(SUCCEEDED(hr));
        teapot_populate_command_list(teapot, teapot->swapchain_images[i].command_list, i);
    }
}

static void teapot_wait_for_previous_frame(struct teapot *teapot)
{
    struct teapot_fence *fence = &teapot->fence;
    const UINT64 v = fence->value++;
    HRESULT hr;

    hr = ID3D12CommandQueue_Signal(teapot->command_queue, fence->fence, v);
    assert(SUCCEEDED(hr));
    hr = ID3D12Fence_SetEventOnCompletion(fence->fence, v, NULL);
    assert(SUCCEEDED(hr));

    teapot->rt_idx = demo_swapchain_get_current_back_buffer_index(teapot->swapchain);
}

static void teapot_update_mvp(struct teapot *teapot)
{
    struct demo_vec3 up = {0.0f, 0.0f, teapot->theta < 0.0f ? -1.0f : 1.0f};
    struct demo_vec3 ref = {0.0f, 0.0f, 1.5f};
    struct demo_matrix projection, world;
    struct demo_vec3 eye;
    float r = 25.0f;

    eye.z = 1.5 + r * cosf(teapot->theta);
    r *= sinf(teapot->theta);
    eye.y = r * sinf(teapot->phi);
    eye.x = r * cosf(teapot->phi);

    demo_matrix_perspective_rh(&projection, 2.0f, 2.0f * teapot->height / teapot->width, 5.0f, 160.0f);
    demo_matrix_look_at_rh(&world, &eye, &ref, &up);
    demo_matrix_multiply(&teapot->cb_data->mvp_matrix, &world, &projection);
    teapot->cb_data->eye = eye;
}

static void teapot_update_text(struct teapot *teapot, double fps)
{
    unsigned int h = teapot->text_scale * 16;
    struct demo_text *text = &teapot->text;
    const char *platform, *device;
    D3D12_DRAW_ARGUMENTS *a;
    size_t pad, l;

    static const struct demo_vec4 amber = {1.0f, 0.69f, 0.0f, 1.0f};

    text->run_count = 0;
    text->char_count = 0;
    text->scale = teapot->text_scale;
    text->reverse = true;

    platform = demo_get_platform_name();
    device = demo_swapchain_get_device_name(teapot->swapchain);
    l = strlen(platform) + 2 + strlen(device);
    pad = (teapot->width / (teapot->text_scale * 9)) + 1;
    demo_text_draw(text, &amber, 0, -1 * h, "%s: %s%*s", platform, device, l < pad ? (int)(pad - l) : 0, "");
    text->reverse = false;
    if (teapot->frame_count >= ARRAY_SIZE(teapot->frame_times))
        demo_text_draw(text, &amber, 0, -2 * h, "%.2f fps", fps);
    if (teapot->display_help)
    {
        demo_text_draw(text, &amber, 0, 5 * h, "ESC: Exit");
        demo_text_draw(text, &amber, 0, 4 * h, " F1: Toggle help");
        demo_text_draw(text, &amber, 0, 3 * h, "-/+: Tessellation level (%u)", teapot->tessellation_level);
        demo_text_draw(text, &amber, 0, 2 * h, "  A: Toggle animation (%s)", teapot->animate ? "on" : "off");
        demo_text_draw(text, &amber, 0, 1 * h, "  F: Toggle flat shading (%s)", teapot->flat ? "on" : "off");
        demo_text_draw(text, &amber, 0, 0 * h, "  W: Toggle wireframe (%s)", teapot->wireframe ? "on" : "off");
    }

    a = text->draw_arguments;
    a->VertexCountPerInstance = 4;
    a->InstanceCount = text->run_count;
    a->StartVertexLocation = 0;
    a->StartInstanceLocation = 0;
}

static void teapot_animate(struct teapot *teapot, const struct timeval *tv)
{
    double dt = timeval_diff(tv, &teapot->frame_times[(teapot->frame_count - 1) % ARRAY_SIZE(teapot->frame_times)]);
    double t = tv->tv_sec + tv->tv_usec / 1000000.0;

    static const double max_theta = 150.0 * M_PI / 180.0;
    static const double min_theta = 30.0 * M_PI / 180.0;
    static const double theta_speed = 10.0; /* °/s */
    static const double phi_speed = -20.0; /* °/s */

    static bool recover;


    if (teapot->theta > max_theta || teapot->theta < -M_PI / 2.0)
    {
        teapot->theta_dir = 2.0f;
        recover = true;
    }
    else if (teapot->theta < min_theta)
    {
        teapot->theta_dir = -2.0f;
        recover = true;
    }

    if (recover)
    {
        double offset = dt * teapot->theta_dir * theta_speed * M_PI / 180.0;
        teapot->theta -= offset;
        if (fabs(teapot->theta - M_PI / 2.0) < fabs(offset))
        {
            teapot->t_animate = -1.0;
            recover = false;
        }
    }
    else
    {
        double theta_range = max_theta - min_theta;
        double d;

        if (teapot->t_animate < 0.0)
        {
            /* Calculate "t" from current "theta" and "theta_dir". */
            d = (teapot->theta - min_theta) / theta_range;
            d = acos(d * 2.0 - 1.0);
            if (teapot->theta_dir < 0.0f)
                d = 2.0 * M_PI - d;
            d = (theta_range / M_PI) / ((theta_speed / d) * M_PI / 180.0);
            teapot->t_animate = t - d;
        }

        d = ((t - teapot->t_animate) * theta_speed * M_PI / 180.0) / (theta_range / M_PI);
        d = (cos(fmod(d, 2.0 * M_PI)) + 1.0) / 2.0;
        d = d * theta_range + min_theta;
        teapot->theta_dir = teapot->theta - d;
        teapot->theta = d;
    }

    if (teapot->theta < -M_PI)
        teapot->theta += 2.0 * M_PI;

    teapot->phi += (phi_speed * M_PI / 180.0) * dt;
    if (teapot->phi > M_PI)
        teapot->phi -= 2.0 * M_PI;

    teapot_update_mvp(teapot);
}

static void teapot_render_frame(struct teapot *teapot)
{
    size_t time_idx = teapot->frame_count % ARRAY_SIZE(teapot->frame_times);
    struct timeval t;

    gettimeofday(&t, NULL);
    if (timeval_diff(&t, &teapot->last_text) > 0.1)
    {
        teapot_update_text(teapot, ARRAY_SIZE(teapot->frame_times) / timeval_diff(&t, &teapot->frame_times[time_idx]));
        teapot->last_text = t;
    }

    if (teapot->animate && teapot->frame_count)
    {
        teapot_animate(teapot, &t);
        teapot_update_mvp(teapot);
    }

    teapot->frame_times[time_idx] = t;
    ++teapot->frame_count;

    ID3D12CommandQueue_ExecuteCommandLists(teapot->command_queue, 1,
            (ID3D12CommandList **)&teapot->swapchain_images[teapot->rt_idx].command_list);
    demo_swapchain_present(teapot->swapchain);
    teapot_wait_for_previous_frame(teapot);
}

static void teapot_destroy_pipeline(struct teapot *teapot)
{
    unsigned int i;

    ID3D12DescriptorHeap_Release(teapot->dsv_heap);
    ID3D12DescriptorHeap_Release(teapot->rtv_heap);
    for (i = 0; i < demo_swapchain_get_back_buffer_count(teapot->swapchain); ++i)
    {
        ID3D12CommandAllocator_Release(teapot->swapchain_images[i].command_allocator);
        ID3D12Resource_Release(teapot->swapchain_images[i].render_target);
    }
    free(teapot->swapchain_images);
    demo_swapchain_destroy(teapot->swapchain);
    ID3D12CommandQueue_Release(teapot->command_queue);
    ID3D12Device_Release(teapot->device);
}

static void teapot_load_pipeline(struct teapot *teapot)
{
    struct demo_swapchain_desc swapchain_desc;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc;
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    unsigned int i, rt_count;
    HRESULT hr;

    hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&teapot->device);
    assert(SUCCEEDED(hr));

    memset(&queue_desc, 0, sizeof(queue_desc));
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = ID3D12Device_CreateCommandQueue(teapot->device, &queue_desc,
            &IID_ID3D12CommandQueue, (void **)&teapot->command_queue);
    assert(SUCCEEDED(hr));

    swapchain_desc.buffer_count = 2;
    swapchain_desc.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchain_desc.width = teapot->width;
    swapchain_desc.height = teapot->height;
    teapot->swapchain = demo_swapchain_create(teapot->command_queue, teapot->window, &swapchain_desc);
    assert(teapot->swapchain);
    rt_count = demo_swapchain_get_back_buffer_count(teapot->swapchain);
    teapot->swapchain_images = calloc(rt_count, sizeof(*teapot->swapchain_images));
    assert(teapot->swapchain_images);
    teapot->rt_idx = demo_swapchain_get_current_back_buffer_index(teapot->swapchain);

    memset(&heap_desc, 0, sizeof(heap_desc));
    heap_desc.NumDescriptors = rt_count;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = ID3D12Device_CreateDescriptorHeap(teapot->device, &heap_desc,
            &IID_ID3D12DescriptorHeap, (void **)&teapot->rtv_heap);
    assert(SUCCEEDED(hr));

    teapot->rtv_descriptor_size = ID3D12Device_GetDescriptorHandleIncrementSize(teapot->device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    rtv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(teapot->rtv_heap);
    for (i = 0; i < rt_count; ++i)
    {
        teapot->swapchain_images[i].render_target = demo_swapchain_get_back_buffer(teapot->swapchain, i);
        ID3D12Device_CreateRenderTargetView(teapot->device,
                teapot->swapchain_images[i].render_target, NULL, rtv_handle);
        rtv_handle.ptr += teapot->rtv_descriptor_size;
        hr = ID3D12Device_CreateCommandAllocator(teapot->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
                &IID_ID3D12CommandAllocator, (void **)&teapot->swapchain_images[i].command_allocator);
        assert(SUCCEEDED(hr));
    }

    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = ID3D12Device_CreateDescriptorHeap(teapot->device, &heap_desc,
            &IID_ID3D12DescriptorHeap, (void **)&teapot->dsv_heap);
    assert(SUCCEEDED(hr));
}

static void teapot_fence_destroy(struct teapot_fence *teapot_fence)
{
    ID3D12Fence_Release(teapot_fence->fence);
}

static void teapot_destroy_assets(struct teapot *teapot)
{
    unsigned int i;

    demo_text_cleanup(&teapot->text);
    teapot_fence_destroy(&teapot->fence);
    ID3D12Resource_Release(teapot->ib);
    ID3D12Resource_Release(teapot->vb);
    ID3D12Resource_Unmap(teapot->cb, 0, NULL);
    ID3D12Resource_Release(teapot->cb);
    ID3D12Resource_Release(teapot->ds);
    for (i = 0; i < demo_swapchain_get_back_buffer_count(teapot->swapchain); ++i)
    {
        ID3D12GraphicsCommandList_Release(teapot->swapchain_images[i].command_list);
    }
    ID3D12PipelineState_Release(teapot->pipeline_state);
    ID3D12RootSignature_Release(teapot->root_signature);
}

static void teapot_fence_create(struct teapot_fence *fence, ID3D12Device *device)
{
    HRESULT hr;

    hr = ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence, (void **)&fence->fence);
    assert(SUCCEEDED(hr));
    fence->value = 1;
}

static void teapot_load_mesh(struct teapot *teapot)
{
    struct demo_patch *patches;
    struct demo_vec3 *vertices;
    size_t patch_count;
    HRESULT hr;

    patch_count = ARRAY_SIZE(teapot_rotate_patches) + ARRAY_SIZE(teapot_flip_patches);

    teapot->vb = create_buffer(teapot->device, sizeof(teapot_control_points));
    teapot->ib = create_buffer(teapot->device, patch_count * sizeof(*patches));

    hr = ID3D12Resource_Map(teapot->vb, 0, &(D3D12_RANGE){0, 0}, (void **)&vertices);
    assert(SUCCEEDED(hr));
    hr = ID3D12Resource_Map(teapot->ib, 0, &(D3D12_RANGE){0, 0}, (void **)&patches);
    assert(SUCCEEDED(hr));

    memcpy(vertices, teapot_control_points, sizeof(teapot_control_points));
    memcpy(patches, teapot_rotate_patches, sizeof(teapot_rotate_patches));
    memcpy(&patches[ARRAY_SIZE(teapot_rotate_patches)], teapot_flip_patches, sizeof(teapot_flip_patches));

    ID3D12Resource_Unmap(teapot->ib, 0, NULL);
    ID3D12Resource_Unmap(teapot->vb, 0, NULL);

    teapot->vbv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(teapot->vb);
    teapot->vbv.StrideInBytes = sizeof(*teapot_control_points);
    teapot->vbv.SizeInBytes = sizeof(teapot_control_points);

    teapot->ibv.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(teapot->ib);
    teapot->ibv.SizeInBytes = patch_count * sizeof(*patches);
    teapot->ibv.Format = DXGI_FORMAT_R16_UINT;
}

static void teapot_load_assets(struct teapot *teapot)
{
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc;
    D3D12_ROOT_PARAMETER root_parameters[1];
    D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle;
    D3D12_RESOURCE_DESC resource_desc;
    ID3DBlob *vs, *hs, *ds, *gs, *ps;
    D3D12_HEAP_PROPERTIES heap_desc;
    D3D12_CLEAR_VALUE clear_value;
    unsigned int i;
    HRESULT hr;

    static const D3D12_INPUT_ELEMENT_DESC il_desc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_parameters[0].Descriptor.ShaderRegister = 0;
    root_parameters[0].Descriptor.RegisterSpace = 0;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    memset(&root_signature_desc, 0, sizeof(root_signature_desc));
    root_signature_desc.NumParameters = 1;
    root_signature_desc.pParameters = root_parameters;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
    hr = demo_create_root_signature(teapot->device, &root_signature_desc, &teapot->root_signature);
    assert(SUCCEEDED(hr));

    hr = D3DCompile(teapot_hlsl, teapot_hlsl_size, "teapot.hlsl",
            NULL, NULL, "vs_main", "vs_5_0", 0, 0, &vs, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DCompile(teapot_hlsl, teapot_hlsl_size, "teapot.hlsl",
            NULL, NULL, "hs_main", "hs_5_0", 0, 0, &hs, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DCompile(teapot_hlsl, teapot_hlsl_size, "teapot.hlsl",
            NULL, NULL, "ds_main", "ds_5_0", 0, 0, &ds, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DCompile(teapot_hlsl, teapot_hlsl_size, "teapot.hlsl",
            NULL, NULL, "gs_main", "gs_5_0", 0, 0, &gs, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DCompile(teapot_hlsl, teapot_hlsl_size, "teapot.hlsl",
            NULL, NULL, "ps_main", "ps_5_0", 0, 0, &ps, NULL);
    assert(SUCCEEDED(hr));

    memset(&pso_desc, 0, sizeof(pso_desc));
    pso_desc.InputLayout.pInputElementDescs = il_desc;
    pso_desc.InputLayout.NumElements = ARRAY_SIZE(il_desc);
    pso_desc.pRootSignature = teapot->root_signature;
    pso_desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
    pso_desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    pso_desc.HS.pShaderBytecode = ID3D10Blob_GetBufferPointer(hs);
    pso_desc.HS.BytecodeLength = ID3D10Blob_GetBufferSize(hs);
    pso_desc.DS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ds);
    pso_desc.DS.BytecodeLength = ID3D10Blob_GetBufferSize(ds);
    pso_desc.GS.pShaderBytecode = ID3D10Blob_GetBufferPointer(gs);
    pso_desc.GS.BytecodeLength = ID3D10Blob_GetBufferSize(gs);
    pso_desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
    pso_desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);

    demo_rasterizer_desc_init_default(&pso_desc.RasterizerState);
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.FrontCounterClockwise = TRUE;
    demo_blend_desc_init_default(&pso_desc.BlendState);
    pso_desc.DepthStencilState.DepthEnable = TRUE;
    pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso_desc.SampleDesc.Count = 1;
    hr = ID3D12Device_CreateGraphicsPipelineState(teapot->device, &pso_desc,
            &IID_ID3D12PipelineState, (void **)&teapot->pipeline_state);
    assert(SUCCEEDED(hr));

    ID3D10Blob_Release(ps);
    ID3D10Blob_Release(gs);
    ID3D10Blob_Release(ds);
    ID3D10Blob_Release(hs);
    ID3D10Blob_Release(vs);

    for (i = 0; i < demo_swapchain_get_back_buffer_count(teapot->swapchain); ++i)
    {
        hr = ID3D12Device_CreateCommandList(teapot->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                teapot->swapchain_images[i].command_allocator, teapot->pipeline_state,
                &IID_ID3D12GraphicsCommandList, (void **)&teapot->swapchain_images[i].command_list);
        assert(SUCCEEDED(hr));
        hr = ID3D12GraphicsCommandList_Close(teapot->swapchain_images[i].command_list);
        assert(SUCCEEDED(hr));
    }

    heap_desc.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_desc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_desc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_desc.CreationNodeMask = 1;
    heap_desc.VisibleNodeMask = 1;

    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Alignment = 0;
    resource_desc.Width = teapot->width;
    resource_desc.Height = teapot->height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_D32_FLOAT;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    clear_value.Format = DXGI_FORMAT_D32_FLOAT;
    clear_value.DepthStencil.Depth = 1.0f;
    clear_value.DepthStencil.Stencil = 0;

    hr = ID3D12Device_CreateCommittedResource(teapot->device, &heap_desc, D3D12_HEAP_FLAG_NONE, &resource_desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, &IID_ID3D12Resource, (void **)&teapot->ds);
    assert(SUCCEEDED(hr));

    dsv_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(teapot->dsv_heap);
    ID3D12Device_CreateDepthStencilView(teapot->device, teapot->ds, NULL, dsv_handle);

    teapot->cb = create_buffer(teapot->device, sizeof(*teapot->cb_data));

    hr = ID3D12Resource_Map(teapot->cb, 0, &(D3D12_RANGE){0, 0}, (void **)&teapot->cb_data);
    assert(SUCCEEDED(hr));
    teapot_update_mvp(teapot);
    teapot->cb_data->level = teapot->tessellation_level;
    teapot->cb_data->wireframe = teapot->wireframe;
    teapot->cb_data->flat = teapot->flat;

    demo_text_init(&teapot->text, teapot->device, teapot->width, teapot->height, teapot->text_scale);
    teapot_load_mesh(teapot);

    teapot_fence_create(&teapot->fence, teapot->device);
    teapot_wait_for_previous_frame(teapot);
}

static void teapot_key_press(struct demo_window *window, demo_key key, void *user_data)
{
    struct teapot *teapot = user_data;

    switch (key)
    {
        case '-':
        case DEMO_KEY_KP_SUBTRACT:
            if (teapot->tessellation_level > 1)
                teapot->cb_data->level = --teapot->tessellation_level;
            break;
        case '=':
        case DEMO_KEY_KP_ADD:
            if (teapot->tessellation_level < D3D12_TESSELLATOR_MAX_TESSELLATION_FACTOR)
                teapot->cb_data->level = ++teapot->tessellation_level;
            break;
        case 'a':
            if ((teapot->animate = !teapot->animate))
                teapot->t_animate = -1.0;
            break;
        case 'f':
            teapot->cb_data->flat = teapot->flat = !teapot->flat;
            break;
        case 'w':
            teapot->cb_data->wireframe = teapot->wireframe = !teapot->wireframe;
            break;
        case DEMO_KEY_ESCAPE:
            demo_window_destroy(window);
            break;
        case DEMO_KEY_LEFT:
            teapot->phi -= M_PI / 36.0f;
            if (teapot->phi < -M_PI)
                teapot->phi += 2.0f * M_PI;
            teapot_update_mvp(teapot);
            break;
        case DEMO_KEY_RIGHT:
            teapot->phi += M_PI / 36.0f;
            if (teapot->phi > M_PI)
                teapot->phi -= 2.0f * M_PI;
            teapot_update_mvp(teapot);
            break;
        case DEMO_KEY_UP:
            teapot->theta -= M_PI / 36.0f;
            if (teapot->theta < -M_PI)
                teapot->theta += 2.0f * M_PI;
            teapot->t_animate = -1.0;
            teapot_update_mvp(teapot);
            break;
        case DEMO_KEY_DOWN:
            teapot->theta += M_PI / 36.0f;
            if (teapot->theta > M_PI)
                teapot->theta -= 2.0f * M_PI;
            teapot->t_animate = -1.0;
            teapot_update_mvp(teapot);
            break;
        case DEMO_KEY_F1:
            teapot->display_help = !teapot->display_help;
            break;
        default:
            break;
    }
}

static void teapot_expose(struct demo_window *window, void *user_data)
{
    teapot_render_frame(user_data);
}

static void teapot_idle(struct demo *demo, void *user_data)
{
    teapot_render_frame(user_data);
}

static int teapot_main(void)
{
    unsigned int width = 800, height = 600;
    struct teapot teapot;
    double dpi_x, dpi_y;

    memset(&teapot, 0, sizeof(teapot));
    if (!demo_init(&teapot.demo, &teapot))
        return EXIT_FAILURE;
    demo_set_idle_func(&teapot.demo, teapot_idle);

    demo_get_dpi(&teapot.demo, &dpi_x, &dpi_y);
    width *= dpi_x / 96.0;
    height *= dpi_y / 96.0;
    teapot.window = demo_window_create(&teapot.demo, "vkd3d teapot", width, height, &teapot);
    demo_window_set_key_press_func(teapot.window, teapot_key_press);
    demo_window_set_expose_func(teapot.window, teapot_expose);

    teapot.width = width;
    teapot.height = height;
    teapot.tessellation_level = 10;
    teapot.text_scale = (1.25 * dpi_y / 96.0) + 0.5;

    teapot.t_animate = -1.0;
    teapot.theta = M_PI / 2.0f;
    teapot.phi = -M_PI / 4.0f;

    teapot.display_help = true;
    teapot.animate = true;

    teapot.vp.Width = width;
    teapot.vp.Height = height;
    teapot.vp.MaxDepth = 1.0f;

    teapot.scissor_rect.right = width;
    teapot.scissor_rect.bottom = height;

    teapot_load_pipeline(&teapot);
    teapot_load_assets(&teapot);
    teapot_populate_command_lists(&teapot);

    printf("vkd3d-teapot: Running on \"%s\" using %s.\n",
            demo_swapchain_get_device_name(teapot.swapchain), demo_get_platform_name());
    demo_process_events(&teapot.demo);

    teapot_wait_for_previous_frame(&teapot);
    teapot_destroy_assets(&teapot);
    teapot_destroy_pipeline(&teapot);
    demo_cleanup(&teapot.demo);

    return EXIT_SUCCESS;
}

#ifdef _WIN32
/* Do not trigger -Wmissing-prototypes. */
int wmain(void);

int wmain(void)
#else
int main(void)
#endif
{
    return teapot_main();
}
