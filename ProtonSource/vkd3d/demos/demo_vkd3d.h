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

#include "config.h"
#define VK_NO_PROTOTYPES
#ifdef __APPLE__
# define VK_USE_PLATFORM_METAL_EXT
#endif
#ifdef _WIN32
# define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifdef HAVE_XCB
# define VK_USE_PLATFORM_XCB_KHR
#endif
#define VKD3D_UTILS_API_VERSION VKD3D_API_VERSION_1_19
#include <vkd3d.h>
#include <vkd3d_utils.h>
#ifdef HAVE_XCB
# include <xcb/xcb_event.h>
# include <xcb/xcb_icccm.h>
# include <xcb/xcb_keysyms.h>
#endif
#ifndef _WIN32
# include <dlfcn.h>
#endif
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define DECLARE_VK_PFN(name) PFN_##name name;
DECLARE_VK_PFN(vkAcquireNextImageKHR)
DECLARE_VK_PFN(vkCreateFence)
DECLARE_VK_PFN(vkCreateSwapchainKHR)
#ifdef __APPLE__
DECLARE_VK_PFN(vkCreateMetalSurfaceEXT)
#endif
#ifdef _WIN32
DECLARE_VK_PFN(vkCreateWin32SurfaceKHR)
#endif
#ifdef HAVE_XCB
DECLARE_VK_PFN(vkCreateXcbSurfaceKHR)
#endif
DECLARE_VK_PFN(vkDestroyFence)
DECLARE_VK_PFN(vkDestroySurfaceKHR)
DECLARE_VK_PFN(vkDestroySwapchainKHR)
DECLARE_VK_PFN(vkGetPhysicalDeviceProperties)
DECLARE_VK_PFN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
DECLARE_VK_PFN(vkGetPhysicalDeviceSurfaceFormatsKHR)
DECLARE_VK_PFN(vkGetPhysicalDeviceSurfaceSupportKHR)
DECLARE_VK_PFN(vkGetSwapchainImagesKHR)
DECLARE_VK_PFN(vkQueuePresentKHR)
DECLARE_VK_PFN(vkResetFences)
DECLARE_VK_PFN(vkWaitForFences)

struct demo_macos
{
#ifdef __APPLE__
#endif
};

struct demo_win32
{
#ifdef _WIN32
    UINT (*GetDpiForSystem)(void);
#endif
};

struct demo_xcb
{
#ifdef HAVE_XCB
    xcb_connection_t *connection;
    xcb_atom_t wm_protocols_atom;
    xcb_atom_t wm_delete_window_atom;
    xcb_key_symbols_t *xcb_keysyms;
    int screen;
#endif
};

struct demo
{
    union
    {
        struct demo_macos macos;
        struct demo_win32 win32;
        struct demo_xcb xcb;
    } u;

    struct demo_window **windows;
    size_t windows_size;
    size_t window_count;

    void *user_data;
    void (*idle_func)(struct demo *demo, void *user_data);
    struct demo_window *(*create_window)(struct demo *demo, const char *title,
            unsigned int width, unsigned int height, void *user_data);
    void (*get_dpi)(struct demo *demo, double *dpi_x, double *dpi_y);
    void (*process_events)(struct demo *demo);
    void (*cleanup)(struct demo *demo);
};

struct demo_window
{
    struct demo *demo;

    void *user_data;
    void (*expose_func)(struct demo_window *window, void *user_data);
    void (*key_press_func)(struct demo_window *window, demo_key key, void *user_data);
    VkSurfaceKHR (*create_vk_surface)(struct demo_window *window, VkInstance vk_instance);
    void (*destroy)(struct demo_window *window);
};

static inline bool demo_add_window(struct demo *demo, struct demo_window *window)
{
    if (demo->window_count == demo->windows_size)
    {
        size_t new_capacity;
        void *new_elements;

        new_capacity = max(demo->windows_size * 2, 4);
        if (!(new_elements = realloc(demo->windows, new_capacity * sizeof(*demo->windows))))
            return false;
        demo->windows = new_elements;
        demo->windows_size = new_capacity;
    }

    demo->windows[demo->window_count++] = window;

    return true;
}

static inline void demo_remove_window(struct demo *demo, const struct demo_window *window)
{
    size_t i;

    for (i = 0; i < demo->window_count; ++i)
    {
        if (demo->windows[i] != window)
            continue;

        --demo->window_count;
        memmove(&demo->windows[i], &demo->windows[i + 1], (demo->window_count - i) * sizeof(*demo->windows));
        break;
    }
}

static inline bool demo_window_init(struct demo_window *window, struct demo *demo, void *user_data,
        VkSurfaceKHR (*create_vk_surface)(struct demo_window *window, VkInstance vk_instance),
        void (*destroy)(struct demo_window *window))
{
    if (!demo_add_window(demo, window))
        return false;

    window->demo = demo;
    window->user_data = user_data;
    window->expose_func = NULL;
    window->key_press_func = NULL;
    window->create_vk_surface = create_vk_surface;
    window->destroy = destroy;

    return true;
}

static inline void demo_window_cleanup(struct demo_window *window)
{
    demo_remove_window(window->demo, window);
}

#ifdef __APPLE__
# include "demo_macos.h"
#endif
#ifdef _WIN32
# include "demo_win32.h"
#endif
#ifdef HAVE_XCB
# include "demo_xcb.h"
#endif

static void load_vulkan_procs(void)
{
    void *libvulkan;

#ifdef _WIN32
    if (!(libvulkan = LoadLibraryA(SONAME_LIBVULKAN)))
    {
        fprintf(stderr, "Failed to load %s.\n", SONAME_LIBVULKAN);
        exit(1);
    }
#else
    if (!(libvulkan = dlopen(SONAME_LIBVULKAN, RTLD_NOW)))
    {
        fprintf(stderr, "Failed to load %s: %s.\n", SONAME_LIBVULKAN, dlerror());
        exit(1);
    }
#endif

#ifdef _WIN32
# define LOAD_VK_PFN(name) name = (void *)GetProcAddress(libvulkan, #name);
#else
# define LOAD_VK_PFN(name) name = (void *)dlsym(libvulkan, #name);
#endif
    LOAD_VK_PFN(vkAcquireNextImageKHR)
    LOAD_VK_PFN(vkCreateFence)
    LOAD_VK_PFN(vkCreateSwapchainKHR)
#ifdef __APPLE__
    LOAD_VK_PFN(vkCreateMetalSurfaceEXT)
#endif
#ifdef _WIN32
    LOAD_VK_PFN(vkCreateWin32SurfaceKHR)
#endif
#ifdef HAVE_XCB
    LOAD_VK_PFN(vkCreateXcbSurfaceKHR)
#endif
    LOAD_VK_PFN(vkDestroyFence)
    LOAD_VK_PFN(vkDestroySurfaceKHR)
    LOAD_VK_PFN(vkDestroySwapchainKHR)
    LOAD_VK_PFN(vkGetPhysicalDeviceProperties)
    LOAD_VK_PFN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    LOAD_VK_PFN(vkGetPhysicalDeviceSurfaceFormatsKHR)
    LOAD_VK_PFN(vkGetPhysicalDeviceSurfaceSupportKHR)
    LOAD_VK_PFN(vkGetSwapchainImagesKHR)
    LOAD_VK_PFN(vkQueuePresentKHR)
    LOAD_VK_PFN(vkResetFences)
    LOAD_VK_PFN(vkWaitForFences)
}

struct demo_swapchain
{
    VkPhysicalDeviceProperties vk_device_properties;
    VkSurfaceKHR vk_surface;
    VkSwapchainKHR vk_swapchain;
    VkFence vk_fence;

    VkInstance vk_instance;
    VkDevice vk_device;
    ID3D12CommandQueue *command_queue;

    ID3D12Fence *present_fence;
    unsigned long long present_count;

    uint32_t current_buffer;
    unsigned int buffer_count;
    ID3D12Resource *buffers[1];
};

static inline void demo_cleanup(struct demo *demo)
{
    free(demo->windows);
    demo->cleanup(demo);
}

static inline bool demo_init(struct demo *demo, void *user_data)
{
#ifdef __APPLE__
    if (demo_macos_init(&demo->u.macos))
    {
        demo->create_window = demo_window_macos_create;
        demo->get_dpi = demo_macos_get_dpi;
        demo->process_events = demo_macos_process_events;
        demo->cleanup = demo_macos_cleanup;
    }
#endif
#ifdef _WIN32
    if (demo_win32_init(&demo->u.win32))
    {
        demo->create_window = demo_window_win32_create;
        demo->get_dpi = demo_win32_get_dpi;
        demo->process_events = demo_win32_process_events;
        demo->cleanup = demo_win32_cleanup;
    }
#endif
#ifdef HAVE_XCB
    if (demo_xcb_init(&demo->u.xcb))
    {
        demo->create_window = demo_window_xcb_create;
        demo->get_dpi = demo_xcb_get_dpi;
        demo->process_events = demo_xcb_process_events;
        demo->cleanup = demo_xcb_cleanup;
    }
#endif
    else
    {
        fprintf(stderr, "Failed to initialise demo.\n");
        return false;
    }

    demo->windows = NULL;
    demo->windows_size = 0;
    demo->window_count = 0;
    demo->user_data = user_data;
    demo->idle_func = NULL;

    return true;
}

static inline void demo_get_dpi(struct demo *demo, double *dpi_x, double *dpi_y)
{
    demo->get_dpi(demo, dpi_x, dpi_y);
}

static inline const char *demo_get_platform_name(void)
{
    return "vkd3d";
}

static inline void demo_process_events(struct demo *demo)
{
    demo->process_events(demo);
}

static inline void demo_set_idle_func(struct demo *demo,
        void (*idle_func)(struct demo *demo, void *user_data))
{
    demo->idle_func = idle_func;
}

static inline void demo_window_destroy(struct demo_window *window)
{
    window->destroy(window);
}

static inline struct demo_window *demo_window_create(struct demo *demo, const char *title,
        unsigned int width, unsigned int height, void *user_data)
{
    return demo->create_window(demo, title, width, height, user_data);
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
    struct vkd3d_image_resource_create_info resource_create_info;
    struct VkSwapchainCreateInfoKHR vk_swapchain_desc;
    VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
    uint32_t format_count, queue_family_index;
    VkSurfaceCapabilitiesKHR surface_caps;
    VkPhysicalDevice vk_physical_device;
    VkFence vk_fence = VK_NULL_HANDLE;
    struct demo_swapchain *swapchain;
    unsigned int image_count, i, j;
    VkFenceCreateInfo fence_desc;
    VkSurfaceFormatKHR *formats;
    ID3D12Device *d3d12_device;
    VkSurfaceKHR vk_surface;
    VkInstance vk_instance;
    VkBool32 supported;
    VkDevice vk_device;
    VkImage *vk_images;
    VkFormat format;

    load_vulkan_procs();

    if ((format = vkd3d_get_vk_format(desc->format)) == VK_FORMAT_UNDEFINED)
        return NULL;

    if (FAILED(ID3D12CommandQueue_GetDevice(command_queue, &IID_ID3D12Device, (void **)&d3d12_device)))
        return NULL;

    vk_instance = vkd3d_instance_get_vk_instance(vkd3d_instance_from_device(d3d12_device));
    vk_physical_device = vkd3d_get_vk_physical_device(d3d12_device);
    vk_device = vkd3d_get_vk_device(d3d12_device);

    if (!(vk_surface = window->create_vk_surface(window, vk_instance)))
    {
        ID3D12Device_Release(d3d12_device);
        return NULL;
    }

    queue_family_index = vkd3d_get_vk_queue_family_index(command_queue);
    if (vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_device,
            queue_family_index, vk_surface, &supported) < 0 || !supported)
        goto fail;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &surface_caps) < 0)
        goto fail;

    image_count = desc->buffer_count;
    if (image_count < surface_caps.minImageCount)
        image_count = surface_caps.minImageCount;
    else if (surface_caps.maxImageCount && image_count > surface_caps.maxImageCount)
        image_count = surface_caps.maxImageCount;

    if (desc->width > surface_caps.maxImageExtent.width || desc->width < surface_caps.minImageExtent.width
            || desc->height > surface_caps.maxImageExtent.height || desc->height < surface_caps.minImageExtent.height
            || !(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
        goto fail;

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, NULL) < 0
            || !format_count || !(formats = calloc(format_count, sizeof(*formats))))
        goto fail;

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, formats) < 0)
    {
        free(formats);
        goto fail;
    }

    if (format_count != 1 || formats->format != VK_FORMAT_UNDEFINED
            || formats->colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
        for (i = 0; i < format_count; ++i)
        {
            if (formats[i].format == format && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                break;
        }

        if (i == format_count)
        {
            free(formats);
            goto fail;
        }
    }

    free(formats);
    formats = NULL;

    vk_swapchain_desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vk_swapchain_desc.pNext = NULL;
    vk_swapchain_desc.flags = 0;
    vk_swapchain_desc.surface = vk_surface;
    vk_swapchain_desc.minImageCount = image_count;
    vk_swapchain_desc.imageFormat = format;
    vk_swapchain_desc.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vk_swapchain_desc.imageExtent.width = desc->width;
    vk_swapchain_desc.imageExtent.height = desc->height;
    vk_swapchain_desc.imageArrayLayers = 1;
    vk_swapchain_desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    vk_swapchain_desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_swapchain_desc.queueFamilyIndexCount = 0;
    vk_swapchain_desc.pQueueFamilyIndices = NULL;
    vk_swapchain_desc.preTransform = surface_caps.currentTransform;
    vk_swapchain_desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vk_swapchain_desc.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    vk_swapchain_desc.clipped = VK_TRUE;
    vk_swapchain_desc.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(vk_device, &vk_swapchain_desc, NULL, &vk_swapchain) < 0)
        goto fail;

    fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_desc.pNext = NULL;
    fence_desc.flags = 0;
    if (vkCreateFence(vk_device, &fence_desc, NULL, &vk_fence) < 0)
        goto fail;

    if (vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, NULL) < 0
            || !(vk_images = calloc(image_count, sizeof(*vk_images))))
        goto fail;

    if (vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &image_count, vk_images) < 0)
    {
        free(vk_images);
        goto fail;
    }

    if (!(swapchain = malloc(offsetof(struct demo_swapchain, buffers[image_count]))))
    {
        free(vk_images);
        goto fail;
    }
    vkGetPhysicalDeviceProperties(vk_physical_device, &swapchain->vk_device_properties);
    swapchain->vk_surface = vk_surface;
    swapchain->vk_swapchain = vk_swapchain;
    swapchain->vk_fence = vk_fence;
    swapchain->vk_instance = vk_instance;
    swapchain->vk_device = vk_device;

    vkAcquireNextImageKHR(vk_device, vk_swapchain, UINT64_MAX,
            VK_NULL_HANDLE, vk_fence, &swapchain->current_buffer);
    vkWaitForFences(vk_device, 1, &vk_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(vk_device, 1, &vk_fence);

    resource_create_info.type = VKD3D_STRUCTURE_TYPE_IMAGE_RESOURCE_CREATE_INFO;
    resource_create_info.next = NULL;
    resource_create_info.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_create_info.desc.Alignment = 0;
    resource_create_info.desc.Width = desc->width;
    resource_create_info.desc.Height = desc->height;
    resource_create_info.desc.DepthOrArraySize = 1;
    resource_create_info.desc.MipLevels = 1;
    resource_create_info.desc.Format = desc->format;
    resource_create_info.desc.SampleDesc.Count = 1;
    resource_create_info.desc.SampleDesc.Quality = 0;
    resource_create_info.desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_create_info.desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    resource_create_info.flags = VKD3D_RESOURCE_INITIAL_STATE_TRANSITION | VKD3D_RESOURCE_PRESENT_STATE_TRANSITION;
    resource_create_info.present_state = D3D12_RESOURCE_STATE_PRESENT;
    for (i = 0; i < image_count; ++i)
    {
        resource_create_info.vk_image = vk_images[i];
        if (FAILED(vkd3d_create_image_resource(d3d12_device, &resource_create_info, &swapchain->buffers[i])))
        {
            for (j = 0; j < i; ++j)
            {
                ID3D12Resource_Release(swapchain->buffers[j]);
            }
            free(swapchain);
            free(vk_images);
            goto fail;
        }
    }
    swapchain->buffer_count = image_count;
    free(vk_images);

    if (FAILED(ID3D12Device_CreateFence(d3d12_device, 0, 0, &IID_ID3D12Fence, (void **)&swapchain->present_fence)))
    {
        for (i = 0; i < image_count; ++i)
        {
            ID3D12Resource_Release(swapchain->buffers[i]);
        }
        free(swapchain);
        goto fail;
    }
    swapchain->present_count = 0;
    ID3D12Device_Release(d3d12_device);

    ID3D12CommandQueue_AddRef(swapchain->command_queue = command_queue);

    return swapchain;

fail:
    if (vk_fence != VK_NULL_HANDLE)
        vkDestroyFence(vk_device, vk_fence, NULL);
    if (vk_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(vk_device, vk_swapchain, NULL);
    vkDestroySurfaceKHR(vk_instance, vk_surface, NULL);
    ID3D12Device_Release(d3d12_device);
    return NULL;
}

static inline const char *demo_swapchain_get_device_name(struct demo_swapchain *swapchain)
{
    return swapchain->vk_device_properties.deviceName;
}

static inline unsigned int demo_swapchain_get_current_back_buffer_index(struct demo_swapchain *swapchain)
{
    return swapchain->current_buffer;
}

static inline ID3D12Resource *demo_swapchain_get_back_buffer(struct demo_swapchain *swapchain, unsigned int index)
{
    ID3D12Resource *resource = NULL;

    if (index < swapchain->buffer_count && (resource = swapchain->buffers[index]))
        ID3D12Resource_AddRef(resource);

    return resource;
}

static inline unsigned int demo_swapchain_get_back_buffer_count(struct demo_swapchain *swapchain)
{
    return swapchain->buffer_count;
}

static inline void demo_swapchain_present(struct demo_swapchain *swapchain)
{
    VkPresentInfoKHR present_desc;
    VkQueue vk_queue;

    present_desc.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_desc.pNext = NULL;
    present_desc.waitSemaphoreCount = 0;
    present_desc.pWaitSemaphores = NULL;
    present_desc.swapchainCount = 1;
    present_desc.pSwapchains = &swapchain->vk_swapchain;
    present_desc.pImageIndices = &swapchain->current_buffer;
    present_desc.pResults = NULL;

    /* Synchronize vkd3d_acquire_vk_queue() with the Direct3D 12 work
     * already submitted to the command queue. */
    ++swapchain->present_count;
    ID3D12CommandQueue_Signal(swapchain->command_queue, swapchain->present_fence, swapchain->present_count);
    ID3D12Fence_SetEventOnCompletion(swapchain->present_fence, swapchain->present_count, NULL);

    vk_queue = vkd3d_acquire_vk_queue(swapchain->command_queue);
    vkQueuePresentKHR(vk_queue, &present_desc);
    vkd3d_release_vk_queue(swapchain->command_queue);

    vkAcquireNextImageKHR(swapchain->vk_device, swapchain->vk_swapchain, UINT64_MAX,
            VK_NULL_HANDLE, swapchain->vk_fence, &swapchain->current_buffer);
    vkWaitForFences(swapchain->vk_device, 1, &swapchain->vk_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(swapchain->vk_device, 1, &swapchain->vk_fence);
}

static inline void demo_swapchain_destroy(struct demo_swapchain *swapchain)
{
    unsigned int i;

    ID3D12CommandQueue_Release(swapchain->command_queue);
    ID3D12Fence_Release(swapchain->present_fence);
    for (i = 0; i < swapchain->buffer_count; ++i)
    {
        ID3D12Resource_Release(swapchain->buffers[i]);
    }
    vkDestroyFence(swapchain->vk_device, swapchain->vk_fence, NULL);
    vkDestroySwapchainKHR(swapchain->vk_device, swapchain->vk_swapchain, NULL);
    vkDestroySurfaceKHR(swapchain->vk_instance, swapchain->vk_surface, NULL);
    free(swapchain);
}

static inline HANDLE demo_create_event(void)
{
    return vkd3d_create_event();
}

static inline unsigned int demo_wait_event(HANDLE event, unsigned int ms)
{
    return vkd3d_wait_event(event, ms);
}

static inline void demo_destroy_event(HANDLE event)
{
    vkd3d_destroy_event(event);
}
