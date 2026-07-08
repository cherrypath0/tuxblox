/*
 * Copyright 2020-2022 Zebediah Figura for CodeWeavers
 * Copyright 2024 Conor McCarthy for CodeWeavers
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

#ifndef __VKD3D_VULKAN_UTILS_H
#define __VKD3D_VULKAN_UTILS_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "vulkan/vulkan.h"
#include "vkd3d_test.h"
#include "utils.h"

/* The helpers in this file are not part of utils.h because vkd3d_api.c
 * needs its own Vulkan helpers specific to API tests. */

#define DECLARE_VK_PFN(name) PFN_##name name;

struct vulkan_test_context
{
    VkInstance instance;
    VkPhysicalDevice phys_device;
    VkDevice device;
    VkQueue queue;
    VkCommandPool command_pool;
    VkCommandBuffer cmd_buffer;
    VkDescriptorPool descriptor_pool;

    uint32_t vk_api_version;
    DECLARE_VK_PFN(vkCreateInstance);
    DECLARE_VK_PFN(vkEnumerateInstanceExtensionProperties);
#define VK_INSTANCE_PFN   DECLARE_VK_PFN
#define VK_DEVICE_PFN     DECLARE_VK_PFN
#include "vulkan_procs.h"
};

#undef DECLARE_VK_PFN

#define VK_CALL(f) (context->f)

static inline void begin_command_buffer(const struct vulkan_test_context *context)
{
    VkCommandBufferBeginInfo buffer_begin_desc = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    VK_CALL(vkBeginCommandBuffer(context->cmd_buffer, &buffer_begin_desc));
}

static inline void end_command_buffer(const struct vulkan_test_context *context)
{
    VkSubmitInfo submit_desc = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};

    VK_CALL(vkEndCommandBuffer(context->cmd_buffer));

    submit_desc.commandBufferCount = 1;
    submit_desc.pCommandBuffers = &context->cmd_buffer;
    VK_CALL(vkQueueSubmit(context->queue, 1, &submit_desc, VK_NULL_HANDLE));
    VK_CALL(vkQueueWaitIdle(context->queue));
}

static inline void transition_image_layout(const struct vulkan_test_context *context,
        VkImage image, VkImageAspectFlags aspect_mask, uint32_t base_layer, uint32_t layer_count,
        VkImageLayout src_layout, VkImageLayout dst_layout)
{
    VkImageMemoryBarrier barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = src_layout;
    barrier.newLayout = dst_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = base_layer;
    barrier.subresourceRange.layerCount = layer_count;

    VK_CALL(vkCmdPipelineBarrier(context->cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier));
}

static inline unsigned int select_vulkan_memory_type(const struct vulkan_test_context *context,
        uint32_t memory_type_mask, VkMemoryPropertyFlags required_flags)
{
    VkPhysicalDeviceMemoryProperties memory_info;
    unsigned int i;

    VK_CALL(vkGetPhysicalDeviceMemoryProperties(context->phys_device, &memory_info));

    for (i = 0; i < memory_info.memoryTypeCount; ++i)
    {
        if (!(memory_type_mask & (1u << i)))
            continue;
        if ((memory_info.memoryTypes[i].propertyFlags & required_flags) == required_flags)
            return i;
    }

    ok(false, "No valid memory types found matching mask %#x, property flags %#x.\n",
            memory_type_mask, required_flags);
    exit(1);
}

static inline VkDeviceMemory allocate_vulkan_device_memory(const struct vulkan_test_context *context,
        const VkMemoryRequirements *memory_reqs, VkMemoryPropertyFlags flags)
{
    VkMemoryAllocateInfo alloc_info = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkDeviceMemory vk_memory;
    VkResult vr;

    alloc_info.allocationSize = memory_reqs->size;
    alloc_info.memoryTypeIndex = select_vulkan_memory_type(context,
            memory_reqs->memoryTypeBits, flags);
    vr = VK_CALL(vkAllocateMemory(context->device, &alloc_info, NULL, &vk_memory));
    ok(vr == VK_SUCCESS, "Got unexpected VkResult %d.\n", vr);

    return vk_memory;
}

static inline VkBuffer create_vulkan_buffer(const struct vulkan_test_context *context, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_flags, VkDeviceMemory *memory)
{
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VkMemoryRequirements memory_reqs;
    VkBuffer buffer;

    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CALL(vkCreateBuffer(context->device, &buffer_info, NULL, &buffer));
    VK_CALL(vkGetBufferMemoryRequirements(context->device, buffer, &memory_reqs));
    *memory = allocate_vulkan_device_memory(context, &memory_reqs, memory_flags);
    VK_CALL(vkBindBufferMemory(context->device, buffer, *memory, 0));

    return buffer;
}

static inline VkBufferView create_vulkan_buffer_view(const struct vulkan_test_context *context,
        VkBuffer buffer, VkFormat format, VkDeviceSize offset)
{
    VkBufferViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    VkBufferView view;

    view_info.buffer = buffer;
    view_info.format = format;
    view_info.offset = offset;
    view_info.range = VK_WHOLE_SIZE;

    VK_CALL(vkCreateBufferView(context->device, &view_info, NULL, &view));

    return view;
}

static inline VkImage create_vulkan_image(const struct vulkan_test_context *context, VkImageType image_type,
        unsigned int width, unsigned int height, unsigned int depth, unsigned int level_count, unsigned int layer_count,
        unsigned int sample_count, VkImageUsageFlags usage, VkFormat format,
        VkImageCreateFlags flags, VkDeviceMemory *memory)
{
    VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    VkMemoryRequirements memory_reqs;
    VkImage image;

    image_info.flags = flags;
    image_info.imageType = image_type;
    image_info.format = format;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = depth;
    image_info.mipLevels = level_count;
    image_info.arrayLayers = layer_count;
    image_info.samples = max(sample_count, 1);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VK_CALL(vkCreateImage(context->device, &image_info, NULL, &image));

    VK_CALL(vkGetImageMemoryRequirements(context->device, image, &memory_reqs));
    *memory = allocate_vulkan_device_memory(context, &memory_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkBindImageMemory(context->device, image, *memory, 0));

    return image;
}

static inline VkImageView create_vulkan_image_view(const struct vulkan_test_context *context, VkImage image,
        VkFormat format, VkImageAspectFlags aspect_mask, VkImageType image_type, bool cube,
        unsigned int layer_count, float min_lod_clamp)
{
    VkImageViewMinLodCreateInfoEXT min_lod_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
    VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkImageView view;

    view_info.image = image;
    if (cube)
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (image_type == VK_IMAGE_TYPE_2D)
        view_info.viewType = (layer_count > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    else if (image_type == VK_IMAGE_TYPE_3D)
        view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = layer_count;

    if (min_lod_clamp != 0.0f)
    {
        min_lod_info.minLod = min_lod_clamp;
        view_info.pNext = &min_lod_info;
    }

    VK_CALL(vkCreateImageView(context->device, &view_info, NULL, &view));
    return view;
}

static inline bool vk_extension_properties_contain(const VkExtensionProperties *extensions,
        uint32_t count, const char *extension_name)
{
    uint32_t i;

    for (i = 0; i < count; ++i)
    {
        if (!strcmp(extensions[i].extensionName, extension_name))
            return true;
    }
    return false;
}

struct vulkan_extension_list
{
    const char **names;
    size_t count;
};

static inline void check_instance_extensions(const struct vulkan_test_context *context,
        const char **instance_extensions, size_t instance_extension_count,
        struct vulkan_extension_list *enabled_extensions)
{
    VkExtensionProperties *extensions;
    uint32_t count, i;

    enabled_extensions->names = calloc(instance_extension_count, sizeof(*enabled_extensions->names));
    enabled_extensions->count = 0;

    VK_CALL(vkEnumerateInstanceExtensionProperties(NULL, &count, NULL));
    extensions = calloc(count, sizeof(*extensions));
    VK_CALL(vkEnumerateInstanceExtensionProperties(NULL, &count, extensions));

    for (i = 0; i < instance_extension_count; ++i)
    {
        const char *name = instance_extensions[i];

        if (vk_extension_properties_contain(extensions, count, name))
            enabled_extensions->names[enabled_extensions->count++] = name;
    }

    free(extensions);
}

static inline bool vulkan_test_context_init_instance(struct vulkan_test_context *context,
        const char **instance_extensions, size_t instance_extension_count)
{
    VkInstanceCreateInfo instance_desc = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    VkApplicationInfo application_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
    struct vulkan_extension_list enabled_extensions;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    uint32_t vk_api_version;
    void *libvulkan;
    uint32_t count;
    VkResult vr;

    memset(context, 0, sizeof(*context));

    if (!(libvulkan = vkd3d_dlopen(SONAME_LIBVULKAN)))
    {
        skip("Failed to load %s: %s.\n", SONAME_LIBVULKAN, vkd3d_dlerror());
        return false;
    }
    vkGetInstanceProcAddr = vkd3d_dlsym(libvulkan, "vkGetInstanceProcAddr");

    context->vkCreateInstance = (void *)vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    context->vkEnumerateInstanceExtensionProperties = (void *)vkGetInstanceProcAddr(NULL,
            "vkEnumerateInstanceExtensionProperties");
    vkEnumerateInstanceVersion = (void *)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");

    if (vkEnumerateInstanceVersion && vkEnumerateInstanceVersion(&vk_api_version) >= 0
            && vk_api_version >= VK_API_VERSION_1_1)
        application_info.apiVersion = VK_API_VERSION_1_1;
    else
        application_info.apiVersion = VK_API_VERSION_1_0;
    context->vk_api_version = application_info.apiVersion;

    instance_desc.pApplicationInfo = &application_info;
    check_instance_extensions(context, instance_extensions, instance_extension_count, &enabled_extensions);
    instance_desc.ppEnabledExtensionNames = enabled_extensions.names;
    instance_desc.enabledExtensionCount = enabled_extensions.count;
    vr = VK_CALL(vkCreateInstance(&instance_desc, NULL, &context->instance));
    free(enabled_extensions.names);
    if (vr < 0)
    {
        skip("Failed to create a Vulkan instance, vr %d.\n", vr);
        return false;
    }

#define VK_INSTANCE_PFN(name) context->name = (void *)vkGetInstanceProcAddr(context->instance, #name);
#include "vulkan_procs.h"

    count = 1;
    if ((vr = VK_CALL(vkEnumeratePhysicalDevices(context->instance, &count, &context->phys_device))) < 0)
    {
        skip("Failed to enumerate physical devices, vr %d.\n", vr);
        goto out_destroy_instance;
    }

    if (!count)
    {
        skip("No Vulkan devices are available.\n");
        goto out_destroy_instance;
    }

    return true;

out_destroy_instance:
    VK_CALL(vkDestroyInstance(context->instance, NULL));
    return false;
}

static inline bool get_vulkan_queue_index(const struct vulkan_test_context *context,
        VkQueueFlags queue_flag, uint32_t *index)
{
    VkQueueFamilyProperties *queue_properties;
    uint32_t count, i;

    count = 0;
    VK_CALL(vkGetPhysicalDeviceQueueFamilyProperties(context->phys_device, &count, NULL));
    queue_properties = malloc(count * sizeof(*queue_properties));
    VK_CALL(vkGetPhysicalDeviceQueueFamilyProperties(context->phys_device, &count, queue_properties));

    for (i = 0; i < count; ++i)
    {
        if (queue_properties[i].queueFlags & queue_flag)
        {
            free(queue_properties);
            *index = i;
            return true;
        }
    }

    free(queue_properties);
    return false;
}

static inline void vulkan_test_context_destroy_device(const struct vulkan_test_context *context)
{
    VkDevice device = context->device;

    VK_CALL(vkDestroyDescriptorPool(device, context->descriptor_pool, NULL));
    VK_CALL(vkFreeCommandBuffers(device, context->command_pool, 1, &context->cmd_buffer));
    VK_CALL(vkDestroyCommandPool(device, context->command_pool, NULL));
    VK_CALL(vkDestroyDevice(device, NULL));
}

static inline bool vulkan_test_context_init_device(struct vulkan_test_context *context,
        const VkDeviceCreateInfo *device_desc, uint32_t queue_index,
        uint32_t max_resource_count, uint32_t max_sampler_count)
{
    VkDescriptorPoolCreateInfo descriptor_pool_desc = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkCommandBufferAllocateInfo cmd_buffer_desc = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    VkCommandPoolCreateInfo command_pool_desc = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    VkDescriptorPoolSize descriptor_pool_sizes[5];
    VkDevice device;
    VkResult vr;

    if ((vr = VK_CALL(vkCreateDevice(context->phys_device, device_desc, NULL, &device))))
    {
        skip("Failed to create device, vr %d.\n", vr);
        return false;
    }
    context->device = device;

#define VK_DEVICE_PFN(name) context->name = (void *)VK_CALL(vkGetDeviceProcAddr(device, #name));
#include "vulkan_procs.h"

    VK_CALL(vkGetDeviceQueue(device, queue_index, 0, &context->queue));

    command_pool_desc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_desc.queueFamilyIndex = queue_index;

    VK_CALL(vkCreateCommandPool(device, &command_pool_desc, NULL, &context->command_pool));

    cmd_buffer_desc.commandPool = context->command_pool;
    cmd_buffer_desc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_desc.commandBufferCount = 1;

    VK_CALL(vkAllocateCommandBuffers(device, &cmd_buffer_desc, &context->cmd_buffer));

    assert(max_resource_count);

    descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptor_pool_sizes[0].descriptorCount = max_resource_count;
    descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_pool_sizes[1].descriptorCount = max_resource_count;
    descriptor_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    descriptor_pool_sizes[2].descriptorCount = max_resource_count;
    descriptor_pool_sizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    descriptor_pool_sizes[3].descriptorCount = max_resource_count;
    descriptor_pool_sizes[4].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptor_pool_sizes[4].descriptorCount = max_sampler_count;

    descriptor_pool_desc.maxSets = 1;
    descriptor_pool_desc.poolSizeCount = ARRAY_SIZE(descriptor_pool_sizes) - !max_sampler_count;
    descriptor_pool_desc.pPoolSizes = descriptor_pool_sizes;

    VK_CALL(vkCreateDescriptorPool(device, &descriptor_pool_desc, NULL, &context->descriptor_pool));

    return true;
}

static inline void vulkan_test_context_destroy(const struct vulkan_test_context *context)
{
    if (context->device)
        vulkan_test_context_destroy_device(context);
    VK_CALL(vkDestroyInstance(context->instance, NULL));
}

/* This doesn't work for NVIDIA or MoltenVK, because they use a different bit pattern. */
static inline void get_vulkan_driver_version(const VkPhysicalDeviceProperties *device_properties,
        const VkPhysicalDeviceDriverPropertiesKHR *driver_properties,
        uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    uint32_t version = device_properties->driverVersion;

    if (version == 1)
    {
        /* llvmpipe doesn't provide a valid driverVersion value, so we resort to
         * parsing the driverInfo string. */
        if (sscanf(driver_properties->driverInfo, "Mesa %u.%u.%u", major, minor, patch) == 3)
            return;
    }

    *major = VK_VERSION_MAJOR(version);
    *minor = VK_VERSION_MINOR(version);
    *patch = VK_VERSION_PATCH(version);
}

static inline bool is_mesa_vulkan_driver(const VkPhysicalDeviceDriverPropertiesKHR *properties)
{
    switch (properties->driverID)
    {
        case VK_DRIVER_ID_MESA_RADV_KHR:
        case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
        case VK_DRIVER_ID_MESA_LLVMPIPE:
        case VK_DRIVER_ID_MESA_TURNIP:
        case VK_DRIVER_ID_MESA_V3DV:
        case VK_DRIVER_ID_MESA_PANVK:
        case VK_DRIVER_ID_MESA_VENUS:
        case VK_DRIVER_ID_MESA_DOZEN:
            return true;

        default:
            return false;
    }
}

static inline bool get_mesa_driver_version(const VkPhysicalDeviceProperties *device_properties,
        const VkPhysicalDeviceDriverPropertiesKHR *driver_properties,
        uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (!is_mesa_vulkan_driver(driver_properties))
        return false;

    get_vulkan_driver_version(device_properties, driver_properties, major, minor, patch);

    return true;
}

static inline bool get_mvk_driver_version(const VkPhysicalDeviceProperties *device_properties,
        const VkPhysicalDeviceDriverPropertiesKHR *driver_properties,
        uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    uint32_t version = device_properties->driverVersion;

    if (driver_properties->driverID != VK_DRIVER_ID_MOLTENVK)
        return false;

    *major = version / 10000;
    *minor = (version / 100) % 100;
    *patch = version % 100;

    return true;
}

static inline bool get_nvidia_driver_version(const VkPhysicalDeviceProperties *device_properties,
        const VkPhysicalDeviceDriverPropertiesKHR *driver_properties,
        uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    uint32_t version = device_properties->driverVersion;

    if (driver_properties->driverID != VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR)
        return false;

    *major = (version >> 22u);
    *minor = (version >> 14u) & ((1u << (22 - 14)) - 1);
    *patch = (version >> 6u) & ((1u << (14 - 6)) - 1);

    return true;
}

static inline bool get_llvm_driver_version(const VkPhysicalDeviceProperties *device_properties,
        uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    return find_version_string(device_properties->deviceName, "LLVM ", major, minor, patch);
}

#endif /* __VKD3D_VULKAN_UTILS_H */
