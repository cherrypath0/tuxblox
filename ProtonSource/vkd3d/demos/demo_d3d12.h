/*
 * Copyright 2016 Józef Kucia for CodeWeavers
 * Copyright 2016 Henri Verbeet for CodeWeavers
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

#include <vkd3d_dxgi1_4.h>
#include <vkd3d_d3dcompiler.h>
#include <stdbool.h>
#include <stdio.h>

struct demo_win32
{
    UINT (*GetDpiForSystem)(void);
};

struct demo
{
    union
    {
        struct demo_win32 win32;
    } u;

    size_t window_count;

    void *user_data;
    void (*idle_func)(struct demo *demo, void *user_data);
};

struct demo_window
{
    struct demo *demo;

    void *user_data;
    void (*expose_func)(struct demo_window *window, void *user_data);
    void (*key_press_func)(struct demo_window *window, demo_key key, void *user_data);
};

static inline bool demo_window_init(struct demo_window *window, struct demo *demo, void *user_data)
{
    window->demo = demo;
    window->user_data = user_data;
    window->expose_func = NULL;
    window->key_press_func = NULL;
    ++demo->window_count;

    return true;
}

static inline void demo_window_cleanup(struct demo_window *window)
{
    --window->demo->window_count;
}

#include "demo_win32.h"

struct demo_swapchain
{
    IDXGISwapChain3 *swapchain;
    unsigned int buffer_count;
    char device_name[128];
};

static inline void demo_cleanup(struct demo *demo)
{
    demo_win32_cleanup(demo);
}

static inline bool demo_init(struct demo *demo, void *user_data)
{
    if (!demo_win32_init(&demo->u.win32))
    {
        fprintf(stderr, "Failed to initialise demo.\n");
        return false;
    }

    demo->window_count = 0;
    demo->user_data = user_data;
    demo->idle_func = NULL;

    return true;
}

static inline void demo_get_dpi(struct demo *demo, double *dpi_x, double *dpi_y)
{
    demo_win32_get_dpi(demo, dpi_x, dpi_y);
}

static inline const char *demo_get_platform_name(void)
{
    return "Direct3D 12";
}

static inline void demo_process_events(struct demo *demo)
{
    demo_win32_process_events(demo);
}

static inline void demo_set_idle_func(struct demo *demo,
        void (*idle_func)(struct demo *demo, void *user_data))
{
    demo->idle_func = idle_func;
}

static inline void demo_window_destroy(struct demo_window *window)
{
    demo_window_win32_destroy(window);
}

static inline struct demo_window *demo_window_create(struct demo *demo, const char *title,
        unsigned int width, unsigned int height, void *user_data)
{
    return demo_window_win32_create(demo, title, width, height, user_data);
}

static inline void demo_window_set_expose_func(struct demo_window *window,
        void (*expose_func)(struct demo_window *window, void *user_data))
{
    window->expose_func = expose_func;
}

static inline void demo_window_set_key_press_func(struct demo_window *window,
        void (*key_press_func)(struct demo_window *window, demo_key key, void *user_data))
{
    window->key_press_func = key_press_func;
}

static inline struct demo_swapchain *demo_swapchain_create(ID3D12CommandQueue *command_queue,
        struct demo_window *window, const struct demo_swapchain_desc *desc)
{
    struct demo_window_win32 *window_win32 = CONTAINING_RECORD(window, struct demo_window_win32, w);
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
    struct demo_swapchain *swapchain;
    DXGI_ADAPTER_DESC adapter_desc;
    IDXGISwapChain1 *swapchain1;
    IDXGIFactory2 *factory;
    IDXGIAdapter *adapter;
    ID3D12Device *device;
    unsigned int i;
    HRESULT hr;
    LUID luid;

    if (!(swapchain = malloc(sizeof(*swapchain))))
        return NULL;

    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory2, (void **)&factory)))
        goto fail;

    if (FAILED(ID3D12CommandQueue_GetDevice(command_queue, &IID_ID3D12Device, (void **)&device)))
        goto fail;
    luid = ID3D12Device_GetAdapterLuid(device);
    ID3D12Device_Release(device);

    sprintf(swapchain->device_name, "Unknown");
    for (i = 0; IDXGIFactory2_EnumAdapters(factory, i, &adapter) == S_OK; ++i)
    {
        hr = IDXGIAdapter_GetDesc(adapter, &adapter_desc);
        IDXGIAdapter_Release(adapter);
        if (FAILED(hr))
            continue;

        if (adapter_desc.AdapterLuid.LowPart == luid.LowPart
                && adapter_desc.AdapterLuid.HighPart == luid.HighPart)
        {
            snprintf(swapchain->device_name, ARRAY_SIZE(swapchain->device_name), "%ls", adapter_desc.Description);
            break;
        }
    }

    memset(&swapchain_desc, 0, sizeof(swapchain_desc));
    swapchain_desc.BufferCount = desc->buffer_count;
    swapchain_desc.Width = desc->width;
    swapchain_desc.Height = desc->height;
    swapchain_desc.Format = desc->format;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.SampleDesc.Count = 1;

    hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)command_queue,
            window_win32->window, &swapchain_desc, NULL, NULL, &swapchain1);
    IDXGIFactory2_Release(factory);
    if (FAILED(hr))
        goto fail;

    swapchain->buffer_count = desc->buffer_count;
    hr = IDXGISwapChain1_QueryInterface(swapchain1, &IID_IDXGISwapChain3, (void **)&swapchain->swapchain);
    IDXGISwapChain1_Release(swapchain1);
    if (FAILED(hr))
        goto fail;

    return swapchain;

fail:
    free(swapchain);
    return NULL;
}

static inline const char *demo_swapchain_get_device_name(struct demo_swapchain *swapchain)
{
    return swapchain->device_name;
}

static inline unsigned int demo_swapchain_get_current_back_buffer_index(struct demo_swapchain *swapchain)
{
    return IDXGISwapChain3_GetCurrentBackBufferIndex(swapchain->swapchain);
}

static inline ID3D12Resource *demo_swapchain_get_back_buffer(struct demo_swapchain *swapchain, unsigned int index)
{
    ID3D12Resource *buffer;

    if (FAILED(IDXGISwapChain3_GetBuffer(swapchain->swapchain, index,
            &IID_ID3D12Resource, (void **)&buffer)))
        return NULL;

    return buffer;
}

static inline unsigned int demo_swapchain_get_back_buffer_count(struct demo_swapchain *swapchain)
{
    return swapchain->buffer_count;
}

static inline void demo_swapchain_present(struct demo_swapchain *swapchain)
{
    IDXGISwapChain3_Present(swapchain->swapchain, 1, 0);
}

static inline void demo_swapchain_destroy(struct demo_swapchain *swapchain)
{
    IDXGISwapChain3_Release(swapchain->swapchain);
    free(swapchain);
}

static inline HANDLE demo_create_event(void)
{
    return CreateEventA(NULL, FALSE, FALSE, NULL);
}

static inline unsigned int demo_wait_event(HANDLE event, unsigned int ms)
{
    return WaitForSingleObject(event, ms);
}

static inline void demo_destroy_event(HANDLE event)
{
    CloseHandle(event);
}
