/*
 * Copyright 2008 Henri Verbeet for CodeWeavers
 * Copyright 2015 Józef Kucia for CodeWeavers
 * Copyright 2021 Zebediah Figura for CodeWeavers
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
#define INITGUID
#define VKD3D_TEST_NO_DEFS
#include <d3d11_4.h>
#define __vkd3d_d3dcommon_h__
#define __vkd3d_dxgibase_h__
#define __vkd3d_dxgiformat_h__
#include "vkd3d_d3dcompiler.h"
#include "shader_runner.h"
#include "vkd3d_test.h"

static HRESULT (WINAPI *pCreateDXGIFactory1)(REFIID iid, void **factory);

static HRESULT (WINAPI *pD3D11CreateDevice)(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driver_type,
        HMODULE swrast, UINT flags, const D3D_FEATURE_LEVEL *feature_levels, UINT levels,
        UINT sdk_version, ID3D11Device **device_out, D3D_FEATURE_LEVEL *obtained_feature_level,
        ID3D11DeviceContext **immediate_context);

struct d3d11_resource
{
    struct resource r;

    ID3D11Resource *resource;
    ID3D11Buffer *buffer;
    ID3D11Texture2D *texture;
    ID3D11Texture3D *texture_3d;
    ID3D11RenderTargetView *rtv;
    ID3D11DepthStencilView *dsv;
    ID3D11ShaderResourceView *srv;
    ID3D11UnorderedAccessView *uav;
    bool is_uav_counter;
};

static struct d3d11_resource *d3d11_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct d3d11_resource, r);
}

struct d3d11_shader_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;

    ID3D11Device *device;
    HWND window;
    IDXGISwapChain *swapchain;
    ID3D11DeviceContext *immediate_context;
    ID3D11RasterizerState *rasterizer_state;
};

static struct d3d11_shader_runner *d3d11_shader_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct d3d11_shader_runner, r);
}

static void set_viewport(ID3D11DeviceContext *context, float x, float y,
        float width, float height, float min_depth, float max_depth)
{
    D3D11_VIEWPORT vp =
    {
        .TopLeftX = x,
        .TopLeftY = y,
        .Width = width,
        .Height = height,
        .MinDepth = min_depth,
        .MaxDepth = max_depth,
    };

    ID3D11DeviceContext_RSSetViewports(context, 1, &vp);
}

static IDXGIAdapter *create_adapter(void)
{
    IDXGIFactory4 *factory4;
    IDXGIFactory *factory;
    IDXGIAdapter *adapter;
    HRESULT hr;

    if (!pCreateDXGIFactory1)
    {
        trace("CreateDXGIFactory1() is not available.\n");
        return NULL;
    }

    if (FAILED(hr = pCreateDXGIFactory1(&IID_IDXGIFactory, (void **)&factory)))
    {
        trace("Failed to create IDXGIFactory, hr %#lx.\n", hr);
        return NULL;
    }

    adapter = NULL;
    if (test_options.use_warp_device)
    {
        if (SUCCEEDED(hr = IDXGIFactory_QueryInterface(factory, &IID_IDXGIFactory4, (void **)&factory4)))
        {
            hr = IDXGIFactory4_EnumWarpAdapter(factory4, &IID_IDXGIAdapter, (void **)&adapter);
            IDXGIFactory4_Release(factory4);
        }
        else
        {
            trace("Failed to get IDXGIFactory4, hr %#lx.\n", hr);
        }
    }
    else
    {
        hr = IDXGIFactory_EnumAdapters(factory, test_options.adapter_idx, &adapter);
    }
    IDXGIFactory_Release(factory);
    if (FAILED(hr))
        trace("Failed to get adapter, hr %#lx.\n", hr);
    return adapter;
}

static void init_adapter_info(void)
{
    char name[MEMBER_SIZE(DXGI_ADAPTER_DESC, Description)];
    IDXGIAdapter *adapter;
    DXGI_ADAPTER_DESC desc;
    unsigned int i;
    HRESULT hr;

    if (!(adapter = create_adapter()))
        return;

    hr = IDXGIAdapter_GetDesc(adapter, &desc);
    ok(hr == S_OK, "Failed to get adapter desc, hr %#lx.\n", hr);

    /* FIXME: Use debugstr_w(). */
    for (i = 0; i < ARRAY_SIZE(desc.Description) && isprint(desc.Description[i]); ++i)
        name[i] = desc.Description[i];
    name[min(i, ARRAY_SIZE(name) - 1)] = '\0';

    trace("Adapter: %s, %04x:%04x.\n", name, desc.VendorId, desc.DeviceId);

    if (desc.VendorId == 0x1414 && desc.DeviceId == 0x008c)
    {
        trace("Using WARP device.\n");
        test_options.use_warp_device = true;
    }

    IDXGIAdapter_Release(adapter);
}

static ID3D11Device *create_device(void)
{
    static const D3D_FEATURE_LEVEL feature_level[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    IDXGIAdapter *adapter;
    ID3D11Device *device;
    UINT flags = 0;
    HRESULT hr;

    if (test_options.enable_debug_layer)
        flags |= D3D11_CREATE_DEVICE_DEBUG;

    if ((adapter = create_adapter()))
    {
        hr = pD3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
                feature_level, ARRAY_SIZE(feature_level), D3D11_SDK_VERSION, &device, NULL, NULL);
        IDXGIAdapter_Release(adapter);
        return SUCCEEDED(hr) ? device : NULL;
    }

    if (SUCCEEDED(pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
            feature_level, ARRAY_SIZE(feature_level), D3D11_SDK_VERSION, &device, NULL, NULL)))
        return device;
    if (SUCCEEDED(pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags,
            feature_level, ARRAY_SIZE(feature_level), D3D11_SDK_VERSION, &device, NULL, NULL)))
        return device;
    if (SUCCEEDED(pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_REFERENCE, NULL, flags,
            feature_level, ARRAY_SIZE(feature_level), D3D11_SDK_VERSION, &device, NULL, NULL)))
        return device;

    return NULL;
}

static IDXGISwapChain *create_swapchain(ID3D11Device *device, HWND window)
{
    DXGI_SWAP_CHAIN_DESC dxgi_desc;
    IDXGISwapChain *swapchain;
    IDXGIDevice *dxgi_device;
    IDXGIAdapter *adapter;
    IDXGIFactory *factory;
    HRESULT hr;

    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "Failed to get DXGI device, hr %#lx.\n", hr);
    hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    ok(hr == S_OK, "Failed to get adapter, hr %#lx.\n", hr);
    IDXGIDevice_Release(dxgi_device);
    hr = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory, (void **)&factory);
    ok(hr == S_OK, "Failed to get factory, hr %#lx.\n", hr);
    IDXGIAdapter_Release(adapter);

    dxgi_desc.BufferDesc.Width = RENDER_TARGET_WIDTH;
    dxgi_desc.BufferDesc.Height = RENDER_TARGET_HEIGHT;
    dxgi_desc.BufferDesc.RefreshRate.Numerator = 60;
    dxgi_desc.BufferDesc.RefreshRate.Denominator = 1;
    dxgi_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dxgi_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    dxgi_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    dxgi_desc.SampleDesc.Count = 1;
    dxgi_desc.SampleDesc.Quality = 0;
    dxgi_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dxgi_desc.BufferCount = 1;
    dxgi_desc.OutputWindow = window;
    dxgi_desc.Windowed = TRUE;
    dxgi_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    dxgi_desc.Flags = 0;

    hr = IDXGIFactory_CreateSwapChain(factory, (IUnknown *)device, &dxgi_desc, &swapchain);
    ok(hr == S_OK, "Failed to create swapchain, hr %#lx.\n", hr);
    IDXGIFactory_Release(factory);

    return swapchain;
}

static bool get_format_support(ID3D11Device *device, enum DXGI_FORMAT format)
{
    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 format_support2 = {.InFormat = format};
    uint32_t ret = 0;
    HRESULT hr;

    hr = ID3D11Device_CheckFeatureSupport(device, D3D11_FEATURE_FORMAT_SUPPORT2,
            &format_support2, sizeof(format_support2));
    ok(hr == S_OK, "Failed to query format support2, hr %#lx.\n", hr);

    if (format_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD)
        ret |= FORMAT_CAP_UAV_LOAD;

    return ret;
}

static bool get_adapter_desc(ID3D11Device *device, DXGI_ADAPTER_DESC *desc)
{
    IDXGIDevice *dxgi_device;
    IDXGIAdapter *adapter;
    HRESULT hr;

    memset(desc, 0, sizeof(*desc));

    if (!vkd3d_test_platform_is_windows())
        return false;

    hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi_device);
    ok(hr == S_OK, "Failed to get DXGI device, hr %#lx.\n", hr);

    hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
    ok(hr == S_OK, "Failed to get DXGI adapter, hr %#lx.\n", hr);

    hr = IDXGIAdapter_GetDesc(adapter, desc);
    ok(hr == S_OK, "Failed to get adapter desc, hr %#lx.\n", hr);

    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);

    return true;
}

static bool is_nvidia_windows_device(ID3D11Device *device)
{
    DXGI_ADAPTER_DESC desc;

    return get_adapter_desc(device, &desc) && desc.VendorId == 0x10de;
}

static BOOL init_test_context(struct d3d11_shader_runner *runner)
{
    D3D11_FEATURE_DATA_D3D11_OPTIONS1 options1 = {0};
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 options2 = {0};
    D3D11_FEATURE_DATA_D3D11_OPTIONS3 options3 = {0};
    D3D11_FEATURE_DATA_DOUBLES doubles = {0};
    unsigned int rt_width, rt_height;
    D3D11_RASTERIZER_DESC rs_desc;
    HRESULT hr;
    RECT rect;

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

    memset(runner, 0, sizeof(*runner));

    if (!(runner->device = create_device()))
    {
        skip("Failed to create device.\n");
        return FALSE;
    }

    hr = ID3D11Device_CheckFeatureSupport(runner->device, D3D11_FEATURE_DOUBLES,
            &doubles, sizeof(doubles));
    ok(hr == S_OK, "Failed to check double precision feature support, hr %#lx.\n", hr);
    hr = ID3D11Device_CheckFeatureSupport(runner->device,
            D3D11_FEATURE_D3D11_OPTIONS1, &options1, sizeof(options1));
    ok(hr == S_OK, "Failed to check feature options1 support, hr %#lx.\n", hr);
    hr = ID3D11Device_CheckFeatureSupport(runner->device,
            D3D11_FEATURE_D3D11_OPTIONS2, &options2, sizeof(options2));
    ok(hr == S_OK, "Failed to check feature options2 support, hr %#lx.\n", hr);
    hr = ID3D11Device_CheckFeatureSupport(runner->device,
            D3D11_FEATURE_D3D11_OPTIONS3, &options3, sizeof(options3));
    ok(hr == S_OK, "Failed to check feature options3 support, hr %#lx.\n", hr);

    runner->caps.runner = "d3d11.dll";
    runner->caps.compiler = HLSL_COMPILER;
    runner->caps.minimum_shader_model = SHADER_MODEL_4_0;
    runner->caps.maximum_shader_model = SHADER_MODEL_5_0;
    runner->caps.shader_caps[SHADER_CAP_CULL_DISTANCE] = true;
    runner->caps.shader_caps[SHADER_CAP_FILTER_MINMAX] = options1.MinMaxFiltering;
    runner->caps.shader_caps[SHADER_CAP_FLOAT64] = doubles.DoublePrecisionFloatShaderOps;
    runner->caps.shader_caps[SHADER_CAP_GEOMETRY_SHADER] = true;
    runner->caps.shader_caps[SHADER_CAP_ROV] = options2.ROVsSupported;
    runner->caps.shader_caps[SHADER_CAP_RT_VP_ARRAY_INDEX] = options3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;
    runner->caps.shader_caps[SHADER_CAP_TESSELLATION_SHADER] = true;
    for (unsigned int i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        runner->caps.format_caps[formats[i]] = get_format_support(runner->device, formats[i]);
    }

    runner->caps.tags[SHADER_RUNNER_TAG_D3D11].present = true;
    runner->caps.tags[SHADER_RUNNER_TAG_WINDOWS].present = true;

    if (test_options.use_warp_device)
        runner->caps.tags[SHADER_RUNNER_TAG_WARP].present = true;

    if (is_nvidia_windows_device(runner->device))
        runner->caps.tags[SHADER_RUNNER_TAG_NVIDIA].present = true;

    rt_width = RENDER_TARGET_WIDTH;
    rt_height = RENDER_TARGET_HEIGHT;
    SetRect(&rect, 0, 0, rt_width, rt_height);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    runner->window = CreateWindowA("static", "d3dcompiler_test", WS_OVERLAPPEDWINDOW,
            0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, NULL, NULL);
    runner->swapchain = create_swapchain(runner->device, runner->window);

    ID3D11Device_GetImmediateContext(runner->device, &runner->immediate_context);

    set_viewport(runner->immediate_context, 0.0f, 0.0f, rt_width, rt_height, 0.0f, 1.0f);

    rs_desc.FillMode = D3D11_FILL_SOLID;
    rs_desc.CullMode = D3D11_CULL_NONE;
    rs_desc.FrontCounterClockwise = FALSE;
    rs_desc.DepthBias = 0;
    rs_desc.DepthBiasClamp = 0.0f;
    rs_desc.SlopeScaledDepthBias = 0.0f;
    rs_desc.DepthClipEnable = TRUE;
    rs_desc.ScissorEnable = FALSE;
    rs_desc.MultisampleEnable = FALSE;
    rs_desc.AntialiasedLineEnable = FALSE;
    hr = ID3D11Device_CreateRasterizerState(runner->device, &rs_desc, &runner->rasterizer_state);
    ok(hr == S_OK, "Failed to create rasterizer state.\n");

    return TRUE;
}

static void destroy_test_context(struct d3d11_shader_runner *runner)
{
    ULONG ref;

    ID3D11RasterizerState_Release(runner->rasterizer_state);
    ID3D11DeviceContext_Release(runner->immediate_context);
    IDXGISwapChain_Release(runner->swapchain);
    DestroyWindow(runner->window);

    ref = ID3D11Device_Release(runner->device);
    ok(!ref, "Device has %lu references left.\n", ref);
}

static ID3D11Buffer *create_buffer(ID3D11Device *device, unsigned int bind_flags, unsigned int size,
        bool is_raw, unsigned int stride, const void *data)
{
    D3D11_SUBRESOURCE_DATA resource_data;
    D3D11_BUFFER_DESC buffer_desc;
    ID3D11Buffer *buffer;
    HRESULT hr;

    buffer_desc.ByteWidth = size;
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = bind_flags;
    buffer_desc.CPUAccessFlags = 0;
    if (is_raw)
        buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    else if (stride)
        buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    else
        buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = stride;

    resource_data.pSysMem = data;
    resource_data.SysMemPitch = 0;
    resource_data.SysMemSlicePitch = 0;

    hr = ID3D11Device_CreateBuffer(device, &buffer_desc, data ? &resource_data : NULL, &buffer);
    ok(hr == S_OK, "Failed to create buffer, hr %#lx.\n", hr);
    return buffer;
}

static unsigned int get_bind_flags(const struct resource_params *params)
{
    if (params->desc.type == RESOURCE_TYPE_UAV)
        return D3D11_BIND_UNORDERED_ACCESS;
    else if (params->desc.type == RESOURCE_TYPE_RENDER_TARGET)
        return D3D11_BIND_RENDER_TARGET;
    else if (params->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        return D3D11_BIND_DEPTH_STENCIL;
    else
        return D3D11_BIND_SHADER_RESOURCE;
}

static void init_subresource_data(D3D11_SUBRESOURCE_DATA *resource_data, const struct resource_params *params)
{
    unsigned int buffer_offset = 0;

    for (unsigned int level = 0; level < params->desc.level_count; ++level)
    {
        unsigned int level_width = get_level_dimension(params->desc.width, level);
        unsigned int level_height = get_level_dimension(params->desc.height, level);
        unsigned int level_depth = get_level_dimension(params->desc.depth, level);

        for (unsigned int layer = 0; layer < params->desc.layer_count; ++layer)
        {
            D3D11_SUBRESOURCE_DATA *subresource = &resource_data[level + layer * params->desc.level_count];
            subresource->pSysMem = &params->data[buffer_offset];
            subresource->SysMemPitch = level_width * params->desc.texel_size;
            subresource->SysMemSlicePitch = level_height * subresource->SysMemPitch;
            buffer_offset += level_depth * subresource->SysMemSlicePitch;
        }
    }
}

static void create_identity_view(ID3D11Device *device,
        struct d3d11_resource *resource, const struct resource_params *params)
{
    HRESULT hr;

    if (params->desc.type == RESOURCE_TYPE_UAV)
        hr = ID3D11Device_CreateUnorderedAccessView(device, resource->resource, NULL, &resource->uav);
    else if (params->desc.type == RESOURCE_TYPE_RENDER_TARGET)
        hr = ID3D11Device_CreateRenderTargetView(device, resource->resource, NULL, &resource->rtv);
    else if (params->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
        hr = ID3D11Device_CreateDepthStencilView(device, resource->resource, NULL, &resource->dsv);
    else
        hr = ID3D11Device_CreateShaderResourceView(device, resource->resource, NULL, &resource->srv);
    ok(hr == S_OK, "Failed to create view, hr %#lx.\n", hr);
}

static bool init_resource_2d(struct d3d11_shader_runner *runner, struct d3d11_resource *resource,
        const struct resource_params *params)
{
    ID3D11Device *device = runner->device;
    D3D11_TEXTURE2D_DESC desc = {0};
    UINT quality_levels;
    HRESULT hr;

    if (params->desc.sample_count > 1)
    {
        if (params->desc.level_count > 1)
            fatal_error("Multisampled texture has multiple levels.\n");

        if (FAILED(ID3D11Device_CheckMultisampleQualityLevels(device,
                params->desc.format, params->desc.sample_count, &quality_levels)) || !quality_levels)
        {
            trace("Format #%x with sample count %u is not supported; skipping.\n",
                    params->desc.format, params->desc.sample_count);
            return false;
        }
    }

    desc.Width = params->desc.width;
    desc.Height = params->desc.height;
    desc.MipLevels = params->desc.level_count;
    desc.ArraySize = params->desc.layer_count;
    desc.Format = params->desc.format;
    desc.SampleDesc.Count = max(params->desc.sample_count, 1);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = get_bind_flags(params);

    if (params->desc.dimension == RESOURCE_DIMENSION_CUBE)
        desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

    if (params->data)
    {
        D3D11_SUBRESOURCE_DATA *resource_data;

        if (params->desc.sample_count > 1)
            fatal_error("Cannot upload data to a multisampled texture.\n");

        resource_data = calloc(params->desc.layer_count * params->desc.level_count, sizeof(*resource_data));
        init_subresource_data(resource_data, params);
        hr = ID3D11Device_CreateTexture2D(device, &desc, resource_data, &resource->texture);
        free(resource_data);
    }
    else
    {
        hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &resource->texture);
    }
    ok(hr == S_OK, "Failed to create texture, hr %#lx.\n", hr);

    resource->resource = (ID3D11Resource *)resource->texture;
    create_identity_view(device, resource, params);
    return true;
}

static bool init_resource_3d(struct d3d11_shader_runner *runner, struct d3d11_resource *resource,
        const struct resource_params *params)
{
    D3D11_SUBRESOURCE_DATA resource_data[6];
    ID3D11Device *device = runner->device;
    D3D11_TEXTURE3D_DESC desc = {0};
    HRESULT hr;

    if (params->desc.level_count > ARRAY_SIZE(resource_data))
        fatal_error("Level count %u is too high.\n", params->desc.level_count);

    desc.Width = params->desc.width;
    desc.Height = params->desc.height;
    desc.Depth = params->desc.depth;
    desc.MipLevels = params->desc.level_count;
    desc.Format = params->desc.format;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = get_bind_flags(params);

    if (params->data)
    {
        init_subresource_data(resource_data, params);
        hr = ID3D11Device_CreateTexture3D(device, &desc, resource_data, &resource->texture_3d);
    }
    else
    {
        hr = ID3D11Device_CreateTexture3D(device, &desc, NULL, &resource->texture_3d);
    }
    ok(hr == S_OK, "Failed to create texture, hr %#lx.\n", hr);

    resource->resource = (ID3D11Resource *)resource->texture_3d;
    create_identity_view(device, resource, params);
    return true;
}

static void init_resource_srv_buffer(struct d3d11_shader_runner *runner, struct d3d11_resource *resource,
        const struct resource_params *params)
{
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ID3D11Device *device = runner->device;
    HRESULT hr;

    resource->buffer = create_buffer(device, D3D11_BIND_SHADER_RESOURCE, params->data_size, params->is_raw,
            params->stride, params->data);
    resource->resource = (ID3D11Resource *)resource->buffer;

    srv_desc.Format = params->desc.format;
    if (params->is_raw)
    {
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srv_desc.BufferEx.FirstElement = 0;
        srv_desc.BufferEx.NumElements = params->data_size / params->desc.texel_size;
        srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    }
    else
    {
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = params->data_size / params->desc.texel_size;
    }
    hr = ID3D11Device_CreateShaderResourceView(device, resource->resource, &srv_desc, &resource->srv);
    ok(hr == S_OK, "Failed to create view, hr %#lx.\n", hr);
}

static void init_resource_uav_buffer(struct d3d11_shader_runner *runner, struct d3d11_resource *resource,
        const struct resource_params *params)
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    ID3D11Device *device = runner->device;
    HRESULT hr;

    resource->buffer = create_buffer(device, D3D11_BIND_UNORDERED_ACCESS, params->data_size, params->is_raw,
            params->stride, params->data);
    resource->resource = (ID3D11Resource *)resource->buffer;
    resource->is_uav_counter = params->is_uav_counter;

    uav_desc.Format = params->desc.format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = params->data_size / params->desc.texel_size;
    if (params->is_raw)
        uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    else if (params->is_uav_counter)
        uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    else
        uav_desc.Buffer.Flags = 0;
    hr = ID3D11Device_CreateUnorderedAccessView(device, resource->resource, &uav_desc, &resource->uav);
    ok(hr == S_OK, "Failed to create view, hr %#lx.\n", hr);
}

static struct resource *d3d11_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    ID3D11Device *device = runner->device;
    struct d3d11_resource *resource;

    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
        case RESOURCE_TYPE_TEXTURE:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
                init_resource_srv_buffer(runner, resource, params);
            else if ((params->desc.dimension == RESOURCE_DIMENSION_2D
                    || params->desc.dimension == RESOURCE_DIMENSION_CUBE)
                    && !init_resource_2d(runner, resource, params))
                return NULL;
            else if (params->desc.dimension == RESOURCE_DIMENSION_3D && !init_resource_3d(runner, resource, params))
                return NULL;
            break;

        case RESOURCE_TYPE_UAV:
            if (params->desc.dimension == RESOURCE_DIMENSION_BUFFER)
                init_resource_uav_buffer(runner, resource, params);
            else if (params->desc.dimension == RESOURCE_DIMENSION_2D && !init_resource_2d(runner, resource, params))
                return NULL;
            else if (params->desc.dimension == RESOURCE_DIMENSION_3D && !init_resource_3d(runner, resource, params))
                return NULL;
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            resource->buffer = create_buffer(device, D3D11_BIND_VERTEX_BUFFER, params->data_size, params->is_raw,
                    params->stride, params->data);
            resource->resource = (ID3D11Resource *)resource->buffer;
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    return &resource->r;
}

static void d3d11_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct d3d11_resource *resource = d3d11_resource(res);

    ID3D11Resource_Release(resource->resource);
    if (resource->rtv)
        ID3D11RenderTargetView_Release(resource->rtv);
    if (resource->dsv)
        ID3D11DepthStencilView_Release(resource->dsv);
    if (resource->srv)
        ID3D11ShaderResourceView_Release(resource->srv);
    if (resource->uav)
        ID3D11UnorderedAccessView_Release(resource->uav);
    free(resource);
}

static ID3D11SamplerState *create_sampler(ID3D11Device *device, const struct sampler *sampler)
{
    ID3D11SamplerState *d3d11_sampler;
    D3D11_SAMPLER_DESC desc = {0};
    HRESULT hr;

    /* Members of D3D11_FILTER are compatible with D3D12_FILTER. */
    desc.Filter = (D3D11_FILTER)sampler->filter;
    /* Members of D3D11_TEXTURE_ADDRESS_MODE are compatible with D3D12_TEXTURE_ADDRESS_MODE. */
    desc.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)sampler->u_address;
    desc.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)sampler->v_address;
    desc.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)sampler->w_address;
    desc.ComparisonFunc = sampler->func;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = ID3D11Device_CreateSamplerState(device, &desc, &d3d11_sampler);
    ok(hr == S_OK, "Failed to create sampler state, hr %#lx.\n", hr);
    return d3d11_sampler;
}

static bool d3d11_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    ID3D11DeviceContext *context = runner->immediate_context;
    ID3D11Device *device = runner->device;
    ID3D11ComputeShader *cs;
    ID3D10Blob *cs_code;
    ID3D11Buffer *cb;
    HRESULT hr;
    size_t i;

    if (!(cs_code = compile_hlsl(&runner->r, SHADER_TYPE_CS)))
        return false;

    hr = ID3D11Device_CreateComputeShader(device, ID3D10Blob_GetBufferPointer(cs_code),
            ID3D10Blob_GetBufferSize(cs_code), NULL, &cs);
    ok(hr == S_OK, "Failed to create compute shader, hr %#lx.\n", hr);

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        cb = create_buffer(device, D3D11_BIND_CONSTANT_BUFFER,
                u->count * sizeof(*u->elements), false, 0, u->elements);
        ID3D11DeviceContext_CSSetConstantBuffers(context, i, 1, &cb);
        ID3D11Buffer_Release(cb);
    }

    for (i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d11_resource *resource = d3d11_resource(runner->r.resources[i]);

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_TEXTURE:
                ID3D11DeviceContext_CSSetShaderResources(context, resource->r.desc.slot, 1, &resource->srv);
                break;

            case RESOURCE_TYPE_UAV:
                ID3D11DeviceContext_CSSetUnorderedAccessViews(context, resource->r.desc.slot, 1, &resource->uav, NULL);
                break;

            case RESOURCE_TYPE_RENDER_TARGET:
            case RESOURCE_TYPE_DEPTH_STENCIL:
            case RESOURCE_TYPE_VERTEX_BUFFER:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        struct sampler *sampler = &runner->r.samplers[i];
        ID3D11SamplerState *d3d11_sampler;

        d3d11_sampler = create_sampler(device, sampler);
        ID3D11DeviceContext_CSSetSamplers(context, sampler->slot, 1, &d3d11_sampler);
        ID3D11SamplerState_Release(d3d11_sampler);
    }

    ID3D11DeviceContext_CSSetShader(context, cs, NULL, 0);
    ID3D11DeviceContext_Dispatch(context, x, y, z);

    ID3D11ComputeShader_Release(cs);

    return true;
}

static void d3d11_runner_clear(struct shader_runner *r, struct resource *res, const struct vec4 *clear_value)
{
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    ID3D11DeviceContext *context = runner->immediate_context;
    struct d3d11_resource *resource = d3d11_resource(res);

    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
            ID3D11DeviceContext_ClearRenderTargetView(context, resource->rtv, (const float *)clear_value);
            break;

        case RESOURCE_TYPE_DEPTH_STENCIL:
            ID3D11DeviceContext_ClearDepthStencilView(context, resource->dsv, D3D11_CLEAR_DEPTH, clear_value->x, 0);
            break;

        default:
            fatal_error("Clears are not implemented for resource type %u.\n", resource->r.desc.type);
    }
}

static bool d3d11_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY primitive_topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    ID3D10Blob *vs_code, *ps_code, *hs_code = NULL, *ds_code = NULL, *gs_code = NULL;
    ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
    ID3D11RenderTargetView *rtvs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {0};
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    ID3D11DeviceContext *context = runner->immediate_context;
    unsigned int fb_width, fb_height, rtv_count = 0;
    unsigned int min_uav_slot = ARRAY_SIZE(uavs);
    ID3D11DepthStencilState *ds_state = NULL;
    D3D11_DEPTH_STENCIL_DESC ds_desc = {0};
    ID3D11Device *device = runner->device;
    ID3D11DepthStencilView *dsv = NULL;
    ID3D11GeometryShader *gs;
    ID3D11Buffer *cb = NULL;
    ID3D11VertexShader *vs;
    ID3D11DomainShader *ds;
    ID3D11PixelShader *ps;
    ID3D11HullShader *hs;
    bool succeeded;
    unsigned int i;
    HRESULT hr;

    vs_code = compile_hlsl(&runner->r, SHADER_TYPE_VS);
    ps_code = compile_hlsl(&runner->r, SHADER_TYPE_PS);
    succeeded = vs_code && ps_code;

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

    hr = ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_code),
            ID3D10Blob_GetBufferSize(vs_code), NULL, &vs);
    ok(hr == S_OK, "Failed to create vertex shader, hr %#lx.\n", hr);

    hr = ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_code),
            ID3D10Blob_GetBufferSize(ps_code), NULL, &ps);
    ok(hr == S_OK, "Failed to create pixel shader, hr %#lx.\n", hr);

    if (hs_code)
    {
        hr = ID3D11Device_CreateHullShader(device, ID3D10Blob_GetBufferPointer(hs_code),
                ID3D10Blob_GetBufferSize(hs_code), NULL, &hs);
        ok(hr == S_OK, "Failed to create hull shader, hr %#lx.\n", hr);
    }
    if (ds_code)
    {
        hr = ID3D11Device_CreateDomainShader(device, ID3D10Blob_GetBufferPointer(ds_code),
                ID3D10Blob_GetBufferSize(ds_code), NULL, &ds);
        ok(hr == S_OK, "Failed to create domain shader, hr %#lx.\n", hr);
    }
    if (gs_code)
    {
        hr = ID3D11Device_CreateGeometryShader(device, ID3D10Blob_GetBufferPointer(gs_code),
                ID3D10Blob_GetBufferSize(gs_code), NULL, &gs);
        ok(hr == S_OK, "Failed to create geometry shader, hr %#lx.\n", hr);
    }

    ID3D10Blob_Release(ps_code);
    if (hs_code)
        ID3D10Blob_Release(hs_code);
    if (ds_code)
        ID3D10Blob_Release(ds_code);
    if (gs_code)
        ID3D10Blob_Release(gs_code);

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        cb = create_buffer(device, D3D11_BIND_CONSTANT_BUFFER,
                u->count * sizeof(*u->elements), false, 0, u->elements);
        ID3D11DeviceContext_VSSetConstantBuffers(context, i, 1, &cb);
        ID3D11DeviceContext_PSSetConstantBuffers(context, i, 1, &cb);
        if (hs_code)
            ID3D11DeviceContext_HSSetConstantBuffers(context, i, 1, &cb);
        if (ds_code)
            ID3D11DeviceContext_DSSetConstantBuffers(context, i, 1, &cb);
        if (gs_code)
            ID3D11DeviceContext_GSSetConstantBuffers(context, i, 1, &cb);
        ID3D11Buffer_Release(cb);
    }

    fb_width = ~0u;
    fb_height = ~0u;
    for (i = 0; i < runner->r.resource_count; ++i)
    {
        struct d3d11_resource *resource = d3d11_resource(runner->r.resources[i]);
        unsigned int stride = get_vb_stride(&runner->r, resource->r.desc.slot);
        unsigned int offset = 0;

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
                rtvs[resource->r.desc.slot] = resource->rtv;
                rtv_count = max(rtv_count, resource->r.desc.slot + 1);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_DEPTH_STENCIL:
                dsv = resource->dsv;
                ds_desc.DepthEnable = TRUE;
                ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
                ds_desc.DepthFunc = runner->r.depth_func;
                hr = ID3D11Device_CreateDepthStencilState(device, &ds_desc, &ds_state);
                ok(hr == S_OK, "Failed to create depth/stencil state, hr %#lx.\n", hr);
                ID3D11DeviceContext_OMSetDepthStencilState(context, ds_state, 0);
                if (resource->r.desc.width < fb_width)
                    fb_width = resource->r.desc.width;
                if (resource->r.desc.height < fb_height)
                    fb_height = resource->r.desc.height;
                break;

            case RESOURCE_TYPE_TEXTURE:
                ID3D11DeviceContext_PSSetShaderResources(context, resource->r.desc.slot, 1, &resource->srv);
                break;

            case RESOURCE_TYPE_UAV:
                uavs[resource->r.desc.slot] = resource->uav;
                min_uav_slot = min(min_uav_slot, resource->r.desc.slot);
                break;

            case RESOURCE_TYPE_VERTEX_BUFFER:
                ID3D11DeviceContext_IASetVertexBuffers(context, resource->r.desc.slot, 1,
                        (ID3D11Buffer **)&resource->resource, &stride, &offset);
                break;

            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }

    ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(context, rtv_count, rtvs, dsv,
            min_uav_slot, ARRAY_SIZE(uavs) - min_uav_slot, &uavs[min_uav_slot], NULL);

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        struct sampler *sampler = &runner->r.samplers[i];
        ID3D11SamplerState *d3d11_sampler;

        d3d11_sampler = create_sampler(device, sampler);
        ID3D11DeviceContext_PSSetSamplers(context, sampler->slot, 1, &d3d11_sampler);
        ID3D11SamplerState_Release(d3d11_sampler);
    }

    if (runner->r.input.element_count)
    {
        D3D11_INPUT_ELEMENT_DESC *descs;
        ID3D11InputLayout *input_layout;

        descs = calloc(runner->r.input.element_count, sizeof(*descs));
        for (i = 0; i < runner->r.input.element_count; ++i)
        {
            const struct input_element *element = &runner->r.input.elements[i];
            D3D11_INPUT_ELEMENT_DESC *desc = &descs[i];

            desc->SemanticName = element->name;
            desc->SemanticIndex = element->index;
            desc->Format = element->format;
            desc->InputSlot = element->slot;
            desc->AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
            desc->InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        }

        hr = ID3D11Device_CreateInputLayout(device, descs, runner->r.input.element_count,
                ID3D10Blob_GetBufferPointer(vs_code), ID3D10Blob_GetBufferSize(vs_code), &input_layout);
        ok(hr == S_OK, "Failed to create input layout, hr %#lx.\n", hr);
        ID3D11DeviceContext_IASetInputLayout(context, input_layout);
        ID3D11InputLayout_Release(input_layout);
    }

    ID3D10Blob_Release(vs_code);

    if (r->sample_mask)
        ID3D11DeviceContext_OMSetBlendState(context, NULL, NULL, r->sample_mask);
    ID3D11DeviceContext_IASetPrimitiveTopology(context, primitive_topology);
    ID3D11DeviceContext_VSSetShader(context, vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(context, ps, NULL, 0);
    if (hs_code)
        ID3D11DeviceContext_HSSetShader(context, hs, NULL, 0);
    if (ds_code)
        ID3D11DeviceContext_DSSetShader(context, ds, NULL, 0);
    if (gs_code)
        ID3D11DeviceContext_GSSetShader(context, gs, NULL, 0);
    ID3D11DeviceContext_RSSetState(context, runner->rasterizer_state);
    set_viewport(context, 0.0f, 0.0f, fb_width, fb_height, 0.0f, 1.0f);

    if (r->viewport_count)
    {
        D3D11_VIEWPORT viewports[ARRAY_SIZE(r->viewports)];
        for (i = 0; i < r->viewport_count; ++i)
        {
            viewports[i].TopLeftX = r->viewports[i].x;
            viewports[i].TopLeftY = r->viewports[i].y;
            viewports[i].Width = r->viewports[i].width;
            viewports[i].Height = r->viewports[i].height;
            viewports[i].MinDepth = 0.0f;
            viewports[i].MaxDepth = 1.0f;
        }
        ID3D11DeviceContext_RSSetViewports(runner->immediate_context, r->viewport_count, viewports);
    }

    ID3D11DeviceContext_DrawInstanced(context, vertex_count, instance_count, first_vertex, first_instance);

    ID3D11PixelShader_Release(ps);
    ID3D11VertexShader_Release(vs);
    if (hs_code)
        ID3D11HullShader_Release(hs);
    if (ds_code)
        ID3D11DomainShader_Release(ds);
    if (gs_code)
        ID3D11GeometryShader_Release(gs);
    if (ds_state)
        ID3D11DepthStencilState_Release(ds_state);

    return true;
}

static bool d3d11_runner_copy(struct shader_runner *r, struct resource *src, struct resource *dst)
{
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    struct d3d11_resource *s = d3d11_resource(src);
    struct d3d11_resource *d = d3d11_resource(dst);

    ID3D11DeviceContext_CopyResource(runner->immediate_context, d->resource, s->resource);

    return true;
}

struct d3d11_resource_readback
{
    struct resource_readback rb;
    ID3D11Resource *resource;
    unsigned int sub_resource_idx;
};

static struct resource_readback *d3d11_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);
    struct d3d11_resource_readback *rb = malloc(sizeof(*rb));
    ID3D11Resource *resolved_resource = NULL, *src_resource;
    struct d3d11_resource *resource = d3d11_resource(res);
    D3D11_MAPPED_SUBRESOURCE map_desc;
    D3D11_BUFFER_DESC buffer_desc;
    bool is_ms = false;
    HRESULT hr;

    src_resource = resource->resource;
    switch (resource->r.desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
        case RESOURCE_TYPE_UAV:
            if (resource->r.desc.dimension == RESOURCE_DIMENSION_BUFFER)
            {
                ID3D11Buffer_GetDesc(resource->buffer, &buffer_desc);
                buffer_desc.Usage = D3D11_USAGE_STAGING;
                buffer_desc.BindFlags = 0;
                buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                buffer_desc.MiscFlags = 0;
                hr = ID3D11Device_CreateBuffer(runner->device, &buffer_desc, NULL, (ID3D11Buffer **)&rb->resource);
                ok(hr == S_OK, "Failed to create buffer, hr %#lx.\n", hr);
            }
            else if (resource->r.desc.dimension == RESOURCE_DIMENSION_2D)
            {
                D3D11_TEXTURE2D_DESC texture_desc;

                ID3D11Texture2D_GetDesc(resource->texture, &texture_desc);
                is_ms = texture_desc.SampleDesc.Count > 1;
                texture_desc.SampleDesc.Count = 1;
                texture_desc.SampleDesc.Quality = 0;
                texture_desc.Usage = D3D11_USAGE_STAGING;
                texture_desc.BindFlags = 0;
                texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                texture_desc.MiscFlags = 0;
                hr = ID3D11Device_CreateTexture2D(runner->device, &texture_desc, NULL, (ID3D11Texture2D **)&rb->resource);
                ok(hr == S_OK, "Failed to create texture, hr %#lx.\n", hr);
                if (is_ms)
                {
                    texture_desc.Usage = D3D11_USAGE_DEFAULT;
                    texture_desc.CPUAccessFlags = 0;
                    hr = ID3D11Device_CreateTexture2D(runner->device, &texture_desc, NULL,
                            (ID3D11Texture2D **)&resolved_resource);
                    ok(hr == S_OK, "Failed to create multisampled texture, hr %#lx.\n", hr);
                    ID3D11DeviceContext_ResolveSubresource(runner->immediate_context, resolved_resource, 0,
                            resource->resource, 0, texture_desc.Format);
                    src_resource = resolved_resource;
                }
            }
            else if (resource->r.desc.dimension == RESOURCE_DIMENSION_3D)
            {
                D3D11_TEXTURE3D_DESC texture_desc;

                ID3D11Texture3D_GetDesc(resource->texture_3d, &texture_desc);
                texture_desc.Usage = D3D11_USAGE_STAGING;
                texture_desc.BindFlags = 0;
                texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                texture_desc.MiscFlags = 0;
                hr = ID3D11Device_CreateTexture3D(runner->device, &texture_desc,
                        NULL, (ID3D11Texture3D **)&rb->resource);
                ok(hr == S_OK, "Failed to create texture, hr %#lx.\n", hr);
            }
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UNIFORM_BUFFER:
            assert(0);
    }

    if (resource->is_uav_counter)
        ID3D11DeviceContext_CopyStructureCount(runner->immediate_context, (ID3D11Buffer *)rb->resource, 0, resource->uav);
    else
        ID3D11DeviceContext_CopyResource(runner->immediate_context, rb->resource, src_resource);
    hr = ID3D11DeviceContext_Map(runner->immediate_context, rb->resource,
            sub_resource_idx, D3D11_MAP_READ, 0, &map_desc);
    ok(hr == S_OK, "Failed to map texture, hr %#lx.\n", hr);

    if (resolved_resource)
        ID3D11Resource_Release(resolved_resource);

    rb->rb.data = map_desc.pData;
    rb->rb.row_pitch = map_desc.RowPitch;
    rb->rb.width = resource->r.desc.width;
    rb->rb.height = resource->r.desc.height;
    rb->rb.depth = resource->r.desc.depth;
    rb->sub_resource_idx = sub_resource_idx;

    return &rb->rb;
}

static void d3d11_runner_release_readback(struct shader_runner *r, struct resource_readback *rb)
{
    struct d3d11_resource_readback *d3d11_rb = CONTAINING_RECORD(rb, struct d3d11_resource_readback, rb);
    struct d3d11_shader_runner *runner = d3d11_shader_runner(r);

    ID3D11DeviceContext_Unmap(runner->immediate_context, d3d11_rb->resource, d3d11_rb->sub_resource_idx);
    ID3D11Resource_Release(d3d11_rb->resource);
    free(d3d11_rb);
}

static const struct shader_runner_ops d3d11_runner_ops =
{
    .create_resource = d3d11_runner_create_resource,
    .destroy_resource = d3d11_runner_destroy_resource,
    .dispatch = d3d11_runner_dispatch,
    .clear = d3d11_runner_clear,
    .draw = d3d11_runner_draw,
    .copy = d3d11_runner_copy,
    .get_resource_readback = d3d11_runner_get_resource_readback,
    .release_readback = d3d11_runner_release_readback,
};

void run_shader_tests_d3d11(void)
{
    struct d3d11_shader_runner runner;
    HMODULE dxgi_module, d3d11_module;

    if (test_skipping_execution("d3d11.dll",
            HLSL_COMPILER, SHADER_MODEL_4_0, SHADER_MODEL_5_0))
        return;

    d3d11_module = LoadLibraryA("d3d11.dll");
    dxgi_module = LoadLibraryA("dxgi.dll");
    if (d3d11_module && dxgi_module)
    {
        pCreateDXGIFactory1 = (void *)GetProcAddress(dxgi_module, "CreateDXGIFactory1");
        pD3D11CreateDevice = (void *)GetProcAddress(d3d11_module, "D3D11CreateDevice");

        init_adapter_info();
        if (init_test_context(&runner))
        {
            run_shader_tests(&runner.r, &runner.caps, &d3d11_runner_ops, NULL);
            destroy_test_context(&runner);
        }
    }
    FreeLibrary(d3d11_module);
    FreeLibrary(dxgi_module);
}

#endif
