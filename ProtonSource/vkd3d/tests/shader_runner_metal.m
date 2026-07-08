/*
 * Copyright 2024 Feifan He for CodeWeavers
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
#import <Metal/Metal.h>
#define COBJMACROS
#define VKD3D_TEST_NO_DEFS
/* Avoid conflicts with the Objective C BOOL definition. */
#define BOOL VKD3D_BOOLEAN
#include "shader_runner.h"
#include "vkd3d_d3dcommon.h"
#undef interface
#undef BOOL

@interface MTLRenderPipelineDescriptor ()
@property (nonatomic, readwrite) NSUInteger sampleMask;
@end

static const MTLResourceOptions DEFAULT_BUFFER_RESOURCE_OPTIONS = MTLResourceCPUCacheModeDefaultCache
        | MTLResourceHazardTrackingModeDefault;

struct metal_resource
{
    struct resource r;

    id<MTLBuffer> buffer;
    id<MTLTexture> texture;
};

struct metal_resource_readback
{
    struct resource_readback rb;
    id<MTLBuffer> buffer;
};

struct metal_runner
{
    struct shader_runner r;
    struct shader_runner_caps caps;

    id<MTLDevice> device;
    id<MTLCommandQueue> queue;

    ID3D10Blob *d3d_blobs[SHADER_TYPE_COUNT];
    struct vkd3d_shader_scan_signature_info signatures[SHADER_TYPE_COUNT];
    struct vkd3d_shader_scan_thread_group_size_info thread_group_size;
};

static MTLPixelFormat get_metal_pixel_format(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return MTLPixelFormatRGBA32Float;
        case DXGI_FORMAT_R32G32B32A32_UINT:
            return MTLPixelFormatRGBA32Uint;
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return MTLPixelFormatRGBA32Sint;
        case DXGI_FORMAT_R32G32_UINT:
            return MTLPixelFormatRG32Uint;
        case DXGI_FORMAT_R32_FLOAT:
            return MTLPixelFormatR32Float;
        case DXGI_FORMAT_R32_SINT:
            return MTLPixelFormatR32Sint;
        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R32_UINT:
            return MTLPixelFormatR32Uint;
        case DXGI_FORMAT_D32_FLOAT:
            return MTLPixelFormatDepth32Float;
        default:
            return MTLPixelFormatInvalid;
    }
}

static MTLVertexFormat get_metal_attribute_format(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return MTLVertexFormatFloat4;
        case DXGI_FORMAT_R32G32B32A32_UINT:
            return MTLVertexFormatUInt4;
        case DXGI_FORMAT_R32G32_FLOAT:
            return MTLVertexFormatFloat2;
        case DXGI_FORMAT_R32G32_SINT:
            return MTLVertexFormatInt2;
        case DXGI_FORMAT_R32_FLOAT:
            return MTLVertexFormatFloat;
        case DXGI_FORMAT_R32_UINT:
            return MTLVertexFormatUInt;
        default:
            return MTLVertexFormatInvalid;
    }
}

static MTLPrimitiveType get_metal_primitive_type(D3D_PRIMITIVE_TOPOLOGY topology)
{
    switch (topology)
    {
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            return MTLPrimitiveTypeTriangle;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
            return MTLPrimitiveTypeTriangleStrip;

        default:
            fatal_error("Unhandled topology %#x.\n", topology);
    }
}

static MTLSamplerAddressMode get_metal_address_mode(D3D12_TEXTURE_ADDRESS_MODE mode)
{
    switch (mode)
    {
        case D3D12_TEXTURE_ADDRESS_MODE_WRAP:
            return MTLSamplerAddressModeRepeat;
        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR:
            return MTLSamplerAddressModeMirrorRepeat;
        case D3D12_TEXTURE_ADDRESS_MODE_CLAMP:
            return MTLSamplerAddressModeClampToEdge;
        case D3D12_TEXTURE_ADDRESS_MODE_BORDER:
            return MTLSamplerAddressModeClampToBorderColor;
        case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE:
            return MTLSamplerAddressModeMirrorClampToEdge;

        default:
            fatal_error("Unhandled address mode %#x.\n", mode);
    }
}

static MTLCompareFunction get_metal_compare_function(D3D12_COMPARISON_FUNC func)
{
    switch (func)
    {
        case VKD3D_SHADER_COMPARISON_FUNC_NEVER:
            return MTLCompareFunctionNever;
        case VKD3D_SHADER_COMPARISON_FUNC_LESS:
            return MTLCompareFunctionLess;
        case VKD3D_SHADER_COMPARISON_FUNC_EQUAL:
            return MTLCompareFunctionEqual;
        case VKD3D_SHADER_COMPARISON_FUNC_LESS_EQUAL:
            return MTLCompareFunctionLessEqual;
        case VKD3D_SHADER_COMPARISON_FUNC_GREATER:
            return MTLCompareFunctionGreater;
        case VKD3D_SHADER_COMPARISON_FUNC_NOT_EQUAL:
            return MTLCompareFunctionNotEqual;
        case VKD3D_SHADER_COMPARISON_FUNC_GREATER_EQUAL:
            return MTLCompareFunctionGreaterEqual;
        case VKD3D_SHADER_COMPARISON_FUNC_ALWAYS:
            return MTLCompareFunctionAlways;
    }
}

static void trace_messages(const char *messages)
{
    const char *p, *end, *line;

    if (!vkd3d_test_state.debug_level)
        return;

    p = messages;
    end = &p[strlen(p)];

    trace("Received messages:\n");
    while (p < end)
    {
        line = p;
        if ((p = memchr(line, '\n', end - line)))
            ++p;
        else
            p = end;
        trace("    %.*s", (int)(p - line), line);
    }
}

static struct metal_resource *metal_resource(struct resource *r)
{
    return CONTAINING_RECORD(r, struct metal_resource, r);
}

static struct metal_runner *metal_runner(struct shader_runner *r)
{
    return CONTAINING_RECORD(r, struct metal_runner, r);
}

static void init_resource_buffer(struct metal_runner *runner,
        struct metal_resource *resource, const struct resource_params *params)
{
    id<MTLDevice> device = runner->device;

    resource->buffer = [device newBufferWithLength:params->data_size
            options:DEFAULT_BUFFER_RESOURCE_OPTIONS | MTLResourceStorageModePrivate];

    if (params->data)
    {
        id<MTLCommandBuffer> command_buffer;
        id<MTLBlitCommandEncoder> blit;
        id<MTLBuffer> upload_buffer;

        upload_buffer = [[device newBufferWithBytes:params->data
                length:params->data_size
                options:DEFAULT_BUFFER_RESOURCE_OPTIONS | MTLResourceStorageModeManaged] autorelease];

        command_buffer = [runner->queue commandBuffer];

        blit = [command_buffer blitCommandEncoder];
        [blit copyFromBuffer:upload_buffer sourceOffset:0 toBuffer:resource->buffer
                destinationOffset:0 size:params->data_size];
        [blit endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
    }
}

static void init_resource_texture(struct metal_runner *runner,
        struct metal_resource *resource, const struct resource_params *params)
{
    id<MTLDevice> device = runner->device;
    MTLTextureDescriptor *desc;

    if (params->desc.sample_count > 1)
    {
        if (params->desc.level_count > 1)
            fatal_error("Multisampled texture has multiple levels.\n");

        if (![device supportsTextureSampleCount:params->desc.sample_count])
        {
            skip("Format #%x with sample count %u is not supported; skipping.\n", params->desc.format,
                    params->desc.sample_count);
            return;
        }
    }

    desc = [[MTLTextureDescriptor alloc] init];
    switch (params->desc.dimension)
    {
        case RESOURCE_DIMENSION_BUFFER:
            desc.textureType = MTLTextureTypeTextureBuffer;
            break;
        case RESOURCE_DIMENSION_2D:
            if (params->desc.sample_count > 1)
                desc.textureType = params->desc.layer_count > 1 ? MTLTextureType2DMultisampleArray
                        : MTLTextureType2DMultisample;
            else
                desc.textureType = params->desc.layer_count > 1 ? MTLTextureType2DArray : MTLTextureType2D;
            break;
        case RESOURCE_DIMENSION_3D:
            desc.textureType = MTLTextureType3D;
            break;
        case RESOURCE_DIMENSION_CUBE:
            desc.textureType = MTLTextureTypeCube;
            break;
        default:
            fatal_error("Unhandled resource dimension %#x.\n", params->desc.dimension);
    }
    desc.pixelFormat = get_metal_pixel_format(params->desc.format);
    ok(desc.pixelFormat != MTLPixelFormatInvalid, "Unhandled pixel format %#x.\n", params->desc.format);
    desc.width = params->desc.width;
    desc.height = params->desc.height;
    desc.depth = params->desc.depth;
    if (params->desc.dimension == RESOURCE_DIMENSION_CUBE)
        desc.arrayLength = params->desc.layer_count / 6;
    else
        desc.arrayLength = params->desc.layer_count;
    desc.mipmapLevelCount = params->desc.level_count;
    desc.sampleCount = max(params->desc.sample_count, 1);
    desc.storageMode = MTLStorageModePrivate;

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
            desc.usage = MTLTextureUsageRenderTarget;
            break;

        case RESOURCE_TYPE_TEXTURE:
            desc.usage = MTLTextureUsageShaderRead;
            break;

        case RESOURCE_TYPE_UAV:
            desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
        case RESOURCE_TYPE_UNIFORM_BUFFER:
            break;
    }

    resource->texture = [device newTextureWithDescriptor:desc];
    ok(resource->texture, "Failed to create texture.\n");

    if (params->data)
    {
        unsigned int buffer_offset = 0, layer, level, level_width, level_height, level_depth;
        id<MTLCommandBuffer> command_buffer;
        id<MTLBlitCommandEncoder> blit;
        id<MTLTexture> upload_texture;

        if (params->desc.sample_count > 1)
            fatal_error("Cannot upload data to a multisampled texture.\n");

        desc.storageMode = MTLStorageModeManaged;
        upload_texture = [[device newTextureWithDescriptor:desc] autorelease];

        for (level = 0; level < params->desc.level_count; ++level)
        {
            level_width  = get_level_dimension(params->desc.width, level);
            level_height = get_level_dimension(params->desc.height, level);
            level_depth = get_level_dimension(params->desc.depth, level);

            for (layer = 0; layer < params->desc.layer_count; ++layer)
            {
                [upload_texture replaceRegion:MTLRegionMake3D(0, 0, 0, level_width, level_height, level_depth)
                        mipmapLevel:level
                        slice:layer
                        withBytes:&params->data[buffer_offset]
                        bytesPerRow:level_width * params->desc.texel_size
                        bytesPerImage:level_height * level_width * params->desc.texel_size];
                buffer_offset += level_depth * level_height * level_width * params->desc.texel_size;
            }
        }

        command_buffer = [runner->queue commandBuffer];

        blit = [command_buffer blitCommandEncoder];
        [blit copyFromTexture:upload_texture toTexture:resource->texture];
        [blit endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
    }

    [desc release];
}

static struct resource *metal_runner_create_resource(struct shader_runner *r, const struct resource_params *params)
{
    struct metal_runner *runner = metal_runner(r);
    struct metal_resource *resource;

    resource = calloc(1, sizeof(*resource));
    init_resource(&resource->r, params);

    switch (params->desc.type)
    {
        case RESOURCE_TYPE_RENDER_TARGET:
        case RESOURCE_TYPE_DEPTH_STENCIL:
        case RESOURCE_TYPE_TEXTURE:
        case RESOURCE_TYPE_UAV:
            init_resource_texture(runner, resource, params);
            break;

        case RESOURCE_TYPE_VERTEX_BUFFER:
            init_resource_buffer(runner, resource, params);
            break;

        case RESOURCE_TYPE_UNIFORM_BUFFER:
            fatal_error("Uniform buffer resource creation not implemented.\n");
    }

    return &resource->r;
}

static void metal_runner_destroy_resource(struct shader_runner *r, struct resource *res)
{
    struct metal_resource *resource = metal_resource(res);

    [resource->texture release];
    [resource->buffer release];
    free(resource);
}

static bool compile_shader(struct metal_runner *runner, enum shader_type type, struct vkd3d_shader_code *out)
{
    struct vkd3d_shader_interface_info interface_info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_INTERFACE_INFO};
    struct vkd3d_shader_resource_binding bindings[MAX_DESCRIPTORS + MAX_SAMPLERS + MAX_UNIFORM_BUFFERS];
    struct vkd3d_shader_compile_info info = {.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
    struct vkd3d_shader_resource_binding *binding;
    unsigned int i, binding_idx = 0;
    char *messages;
    int ret;

    const struct vkd3d_shader_compile_option options[] =
    {
        {VKD3D_SHADER_COMPILE_OPTION_API_VERSION, VKD3D_SHADER_API_VERSION_CURRENT},
        {VKD3D_SHADER_COMPILE_OPTION_FEATURE, shader_runner_caps_get_feature_flags(&runner->caps)},
    };

    if (!(runner->d3d_blobs[type] = compile_hlsl(&runner->r, type)))
        return false;

    info.next = &interface_info;
    info.source.code = ID3D10Blob_GetBufferPointer(runner->d3d_blobs[type]);
    info.source.size = ID3D10Blob_GetBufferSize(runner->d3d_blobs[type]);
    if (runner->r.shader_model < SHADER_MODEL_6_0)
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_TPF;
    else
        info.source_type = VKD3D_SHADER_SOURCE_DXBC_DXIL;
    info.target_type = VKD3D_SHADER_TARGET_MSL;
    info.options = options;
    info.option_count = ARRAY_SIZE(options);
    info.log_level = VKD3D_SHADER_LOG_WARNING;

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        binding = &bindings[interface_info.binding_count];
        binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_CBV;
        binding->register_space = 0;
        binding->register_index = i;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER;
        binding->binding.set = 0;
        binding->binding.binding = binding_idx;
        binding->binding.count = 1;

        ++interface_info.binding_count;
        ++binding_idx;
    }

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *descriptor = &runner->r.descriptors[i];

        binding = &bindings[interface_info.binding_count];
        binding->type = descriptor->type;
        binding->register_index = descriptor->register_idx;
        binding->register_space = descriptor->register_space;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = VKD3D_SHADER_BINDING_FLAG_BUFFER | VKD3D_SHADER_BINDING_FLAG_IMAGE;
        binding->binding.set = 0;
        binding->binding.binding = binding_idx;
        binding->binding.count = descriptor->count;

        ++interface_info.binding_count;
        binding_idx += descriptor->count;
    }

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        binding = &bindings[interface_info.binding_count];
        binding->type = VKD3D_SHADER_DESCRIPTOR_TYPE_SAMPLER;
        binding->register_space = 0;
        binding->register_index = runner->r.samplers[i].slot;
        binding->shader_visibility = VKD3D_SHADER_VISIBILITY_ALL;
        binding->flags = 0;
        binding->binding.set = 0;
        binding->binding.binding = binding_idx;
        binding->binding.count = 1;

        ++interface_info.binding_count;
        ++binding_idx;
    }

    interface_info.bindings = bindings;
    interface_info.next = &runner->thread_group_size;

    runner->thread_group_size.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_THREAD_GROUP_SIZE_INFO;
    runner->thread_group_size.next = &runner->signatures[type];

    runner->signatures[type].type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_SIGNATURE_INFO;
    runner->signatures[type].next = NULL;

    ret = vkd3d_shader_compile(&info, out, &messages);
    if (messages)
        trace_messages(messages);
    vkd3d_shader_free_messages(messages);

    return ret >= 0;
}

static id<MTLFunction> compile_stage(struct metal_runner *runner, enum shader_type type)
{
    struct vkd3d_shader_code out;
    id<MTLFunction> function;
    id<MTLLibrary> library;
    NSString *src;
    NSError *err;

    if (!compile_shader(runner, type, &out))
        return nil;
    src = [[[NSString alloc] initWithBytes:out.code length:out.size encoding:NSUTF8StringEncoding] autorelease];
    library = [[runner->device newLibraryWithSource:src options:nil error:&err] autorelease];
    ok(library, "Failed to create MTLLibrary.\n");
    if (err)
        trace_messages([err.localizedDescription UTF8String]);
    function = [library newFunctionWithName:@"shader_entry"];
    ok(function, "Failed to create MTLFunction.\n");
    vkd3d_shader_free_shader_code(&out);

    return [function autorelease];
}

static struct metal_resource *find_resource(struct metal_runner *runner,
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

    resource = shader_runner_get_resource(&runner->r, resource_type, descriptor->target_idx + offset);

    if (!resource)
        return NULL;

    return metal_resource(resource);
}

static bool encode_argument_buffer(struct metal_runner *runner, id<MTLCommandEncoder> command_encoder,
        void (*use_resource)(id<MTLCommandEncoder> encoder, id<MTLResource> resource, MTLResourceUsage usage),
        const id<MTLSamplerState> *samplers, id<MTLBuffer> *argument_buffer)
{
    NSMutableArray<MTLArgumentDescriptor *> *argument_descriptors;
    id<MTLDevice> device = runner->device;
    MTLArgumentDescriptor *arg_desc;
    id<MTLArgumentEncoder> encoder;
    unsigned int i, j, index = 0;

    argument_descriptors = [[[NSMutableArray alloc] init] autorelease];

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];

        if (!u->count)
            continue;

        arg_desc = [MTLArgumentDescriptor argumentDescriptor];
        arg_desc.dataType = MTLDataTypePointer;
        arg_desc.index = [argument_descriptors count];
        arg_desc.access = MTLBindingAccessReadOnly;
        [argument_descriptors addObject:arg_desc];
    }

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *descriptor = &runner->r.descriptors[i];

        for (j = 0; j < descriptor->count; ++j)
        {
            const struct metal_resource *resource = find_resource(runner, descriptor, j);

            arg_desc = [MTLArgumentDescriptor argumentDescriptor];
            arg_desc.dataType = MTLDataTypeTexture;
            arg_desc.index = [argument_descriptors count];
            arg_desc.access = descriptor->type == VKD3D_SHADER_DESCRIPTOR_TYPE_SRV ?
                    MTLBindingAccessReadOnly : MTLBindingAccessReadWrite;
            arg_desc.textureType = resource ? [resource->texture textureType] : MTLTextureType2D;
            [argument_descriptors addObject:arg_desc];
        }
    }

    for (i = 0; i < runner->r.sampler_count; ++i)
    {
        arg_desc = [MTLArgumentDescriptor argumentDescriptor];
        arg_desc.dataType = MTLDataTypeSampler;
        arg_desc.index = [argument_descriptors count];
        arg_desc.access = MTLBindingAccessReadOnly;
        [argument_descriptors addObject:arg_desc];
    }

    if (![argument_descriptors count])
    {
        *argument_buffer = nil;
        return true;
    }

    encoder = [[device newArgumentEncoderWithArguments:argument_descriptors] autorelease];
    *argument_buffer = [device newBufferWithLength:encoder.encodedLength
            options:DEFAULT_BUFFER_RESOURCE_OPTIONS | MTLResourceStorageModeManaged];
    [encoder setArgumentBuffer:*argument_buffer offset:0];

    for (i = 0; i < ARRAY_SIZE(runner->r.uniform_buffers); ++i)
    {
        const struct uniform_buffer *u = &runner->r.uniform_buffers[i];
        id<MTLBuffer> cb;

        if (!u->count)
            continue;

        cb = [[device newBufferWithBytes:u->elements length:u->count * sizeof(*u->elements)
                options:DEFAULT_BUFFER_RESOURCE_OPTIONS | MTLResourceStorageModeManaged] autorelease];
        [encoder setBuffer:cb offset:0 atIndex:index++];
        use_resource(command_encoder, cb, MTLResourceUsageRead);
    }

    for (i = 0; i < runner->r.descriptor_count; ++i)
    {
        const struct descriptor_mapping *descriptor = &runner->r.descriptors[i];
        MTLResourceUsage usage;

        usage = descriptor->type == VKD3D_SHADER_DESCRIPTOR_TYPE_SRV ?
                MTLResourceUsageRead : MTLResourceUsageRead | MTLResourceUsageWrite;

        for (j = 0; j < descriptor->count; ++j)
        {
            const struct metal_resource *resource = find_resource(runner, descriptor, j);

            if (resource)
            {
                [encoder setTexture:resource->texture atIndex:index];
                use_resource(command_encoder, resource->texture, usage);
            }

            ++index;
        }
    }

    if (samplers)
    {
        for (i = 0; i < runner->r.sampler_count; ++i)
        {
            [encoder setSamplerState:samplers[i] atIndex:index++];
        }
    }

    [*argument_buffer didModifyRange:NSMakeRange(0, encoder.encodedLength)];

    return true;
}

static void use_compute_resource(id<MTLCommandEncoder> encoder, id<MTLResource> resource, MTLResourceUsage usage)
{
    [(id<MTLComputeCommandEncoder>)encoder useResource:resource usage:usage];
}

static bool metal_runner_dispatch(struct shader_runner *r, unsigned int x, unsigned int y, unsigned int z)
{
    struct metal_runner *runner = metal_runner(r);
    id<MTLDevice> device = runner->device;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputePipelineState> pso;
    id<MTLBuffer> argument_buffer;
    id<MTLFunction> cs;
    NSError *err;

    if (!(cs = compile_stage(runner, SHADER_TYPE_CS)))
    {
        trace("Failed to compile kernel function.\n");
        return false;
    }

    pso = [device newComputePipelineStateWithFunction:cs error:&err];
    [cs release];
    if (!pso)
    {
        trace("Failed to compile pipeline state.\n");
        if (err)
        {
            trace_messages([err.localizedDescription UTF8String]);
            [err release];
        }
        return false;
    }

    command_buffer = [runner->queue commandBuffer];

    encoder = [command_buffer computeCommandEncoder];

    if (!encode_argument_buffer(runner, encoder, use_compute_resource, NULL, &argument_buffer))
    {
        [encoder endEncoding];
        [encoder release];
        [command_buffer release];
        [pso release];
        return false;
    }
    [encoder setBuffer:argument_buffer offset:0 atIndex:0];

    [encoder setComputePipelineState:pso];
    [encoder dispatchThreadgroups:MTLSizeMake(x, y, z)
            threadsPerThreadgroup:MTLSizeMake(runner->thread_group_size.x,
                    runner->thread_group_size.y, runner->thread_group_size.z)];

    [encoder endEncoding];
    [encoder release];

    [command_buffer commit];
    [command_buffer waitUntilCompleted];
    [command_buffer release];

    [argument_buffer release];
    [pso release];

    return true;
}

static void metal_runner_clear(struct shader_runner *r, struct resource *res, const struct vec4 *clear_value)
{
    struct metal_resource *resource = metal_resource(res);
    struct metal_runner *runner = metal_runner(r);
    id<MTLRenderCommandEncoder> encoder;
    id<MTLCommandBuffer> command_buffer;
    MTLRenderPassDescriptor *descriptor;

    @autoreleasepool
    {
        descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        command_buffer = [runner->queue commandBuffer];

        switch (resource->r.desc.type)
        {
            case RESOURCE_TYPE_RENDER_TARGET:
                descriptor.colorAttachments[0].texture = resource->texture;
                descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                descriptor.colorAttachments[0].clearColor =
                        MTLClearColorMake(clear_value->x, clear_value->y, clear_value->z, clear_value->w);
                break;
            case RESOURCE_TYPE_DEPTH_STENCIL:
                descriptor.depthAttachment.texture = resource->texture;
                descriptor.depthAttachment.loadAction = MTLLoadActionClear;
                descriptor.depthAttachment.storeAction = MTLStoreActionStore;
                descriptor.depthAttachment.clearDepth = clear_value->x;
                break;
            default:
                fatal_error("Clears are not implemented for resource type %#x.\n", resource->r.desc.type);
        }

        encoder = [command_buffer renderCommandEncoderWithDescriptor:descriptor];
        [encoder endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
    }
}

static void use_graphics_resource(id<MTLCommandEncoder> encoder, id<MTLResource> resource, MTLResourceUsage usage)
{
    [(id<MTLRenderCommandEncoder>)encoder useResource:resource
                                                usage:usage
                                               stages:MTLRenderStageVertex | MTLRenderStageFragment];
}

static bool metal_runner_draw(struct shader_runner *r, D3D_PRIMITIVE_TOPOLOGY topology,
        unsigned int vertex_count, unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance)
{
    MTLViewport viewport = {0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    MTLRenderPassColorAttachmentDescriptor *attachment;
    unsigned int fb_width, fb_height, vb_idx, i, j;
    struct metal_runner *runner = metal_runner(r);
    MTLRenderPipelineDescriptor *pipeline_desc;
    id<MTLSamplerState> samplers[MAX_SAMPLERS];
    MTLVertexBufferLayoutDescriptor *binding;
    id<MTLDepthStencilState> ds_state = nil;
    id<MTLDevice> device = runner->device;
    size_t attribute_offsets[32], stride;
    id<MTLRenderCommandEncoder> encoder;
    id<MTLCommandBuffer> command_buffer;
    id<MTLBuffer> argument_buffer = nil;
    MTLDepthStencilDescriptor *ds_desc;
    MTLRenderPassDescriptor *pass_desc;
    MTLSamplerDescriptor *sampler_desc;
    MTLVertexDescriptor *vertex_desc;
    struct metal_resource *resource;
    id<MTLRenderPipelineState> pso;
    const struct sampler *sampler;
    bool ret = false;
    NSError *err;

    struct
    {
        id<MTLBuffer> buffer;
        unsigned int idx;
    } vb_info[MAX_RESOURCES];

    @autoreleasepool
    {
        pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
        pipeline_desc = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
        vertex_desc = [MTLVertexDescriptor vertexDescriptor];

        if (!(pipeline_desc.vertexFunction = compile_stage(runner, SHADER_TYPE_VS)))
        {
            trace("Failed to compile vertex function.\n");
            goto done;
        }

        if (!(pipeline_desc.fragmentFunction = compile_stage(runner, SHADER_TYPE_PS)))
        {
            trace("Failed to compile fragment function.\n");
            goto done;
        }

        sampler_desc = [[MTLSamplerDescriptor new] autorelease];
        for (i = 0; i < runner->r.sampler_count; ++i)
        {
            sampler = &runner->r.samplers[i];
            sampler_desc.sAddressMode = get_metal_address_mode(sampler->u_address);
            sampler_desc.tAddressMode = get_metal_address_mode(sampler->v_address);
            sampler_desc.rAddressMode = get_metal_address_mode(sampler->w_address);
            sampler_desc.magFilter = (sampler->filter & 0x4)
                    ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
            sampler_desc.minFilter = (sampler->filter & 0x10)
                    ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
            sampler_desc.mipFilter = (sampler->filter & 0x1)
                    ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
            sampler_desc.compareFunction = sampler->func
                    ? get_metal_compare_function(sampler->func) : MTLCompareFunctionNever;
            sampler_desc.supportArgumentBuffers = true;
            samplers[i] = [[device newSamplerStateWithDescriptor:sampler_desc] autorelease];
        }

        fb_width = ~0u;
        fb_height = ~0u;
        /* [[buffer(0)]] is used for the descriptor argument buffer. */
        vb_idx = 1;
        memset(vb_info, 0, sizeof(vb_info));
        for (i = 0; i < runner->r.resource_count; ++i)
        {
            resource = metal_resource(runner->r.resources[i]);
            switch (resource->r.desc.type)
            {
                case RESOURCE_TYPE_RENDER_TARGET:
                    pipeline_desc.colorAttachments[resource->r.desc.slot].pixelFormat = resource->texture.pixelFormat;
                    attachment = pass_desc.colorAttachments[resource->r.desc.slot];
                    attachment.loadAction = MTLLoadActionLoad;
                    attachment.storeAction = MTLStoreActionStore;
                    attachment.texture = resource->texture;
                    if (resource->r.desc.width < fb_width)
                        fb_width = resource->r.desc.width;
                    if (resource->r.desc.height < fb_height)
                        fb_height = resource->r.desc.height;
                    break;

                case RESOURCE_TYPE_DEPTH_STENCIL:
                    pipeline_desc.depthAttachmentPixelFormat = resource->texture.pixelFormat;
                    pass_desc.depthAttachment.loadAction = MTLLoadActionLoad;
                    pass_desc.depthAttachment.storeAction = MTLStoreActionStore;
                    pass_desc.depthAttachment.texture = resource->texture;
                    if (resource->r.desc.width < fb_width)
                        fb_width = resource->r.desc.width;
                    if (resource->r.desc.height < fb_height)
                        fb_height = resource->r.desc.height;

                    ds_desc = [[[MTLDepthStencilDescriptor alloc] init] autorelease];
                    ds_desc.depthCompareFunction = get_metal_compare_function(runner->r.depth_func);
                    ds_desc.depthWriteEnabled = true;
                    ds_state = [[device newDepthStencilStateWithDescriptor:ds_desc] autorelease];
                    break;

                case RESOURCE_TYPE_VERTEX_BUFFER:
                    assert(resource->r.desc.slot < ARRAY_SIZE(vb_info));
                    for (j = 0, stride = 0; j < runner->r.input.element_count; ++j)
                    {
                        if (runner->r.input.elements[j].slot != resource->r.desc.slot)
                            continue;
                        assert(j < ARRAY_SIZE(attribute_offsets));
                        attribute_offsets[j] = stride;
                        stride += runner->r.input.elements[j].texel_size;
                    }
                    if (!stride)
                        break;
                    vb_info[resource->r.desc.slot].buffer = resource->buffer;
                    vb_info[resource->r.desc.slot].idx = vb_idx;
                    binding = [vertex_desc.layouts objectAtIndexedSubscript:vb_idx];
                    binding.stepFunction = MTLVertexStepFunctionPerVertex;
                    binding.stride = stride;
                    ++vb_idx;
                    break;

                default:
                    break;
            }
        }
        pipeline_desc.rasterSampleCount = runner->r.sample_count;
        pipeline_desc.sampleMask = runner->r.sample_mask;
        viewport.width = fb_width;
        viewport.height = fb_height;

        command_buffer = [runner->queue commandBuffer];
        encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];

        if (!encode_argument_buffer(runner, encoder, use_graphics_resource, samplers, &argument_buffer))
        {
            [encoder endEncoding];
            ret = false;
            goto done;
        }
        [encoder setVertexBuffer:argument_buffer offset:0 atIndex:0];
        [encoder setFragmentBuffer:argument_buffer offset:0 atIndex:0];
        [argument_buffer autorelease];

        if (runner->r.input.element_count > 32)
            fatal_error("Unsupported input element count %zu.\n", runner->r.input.element_count);

        for (i = 0; i < runner->r.input.element_count; ++i)
        {
            const struct input_element *element = &runner->r.input.elements[i];
            const struct vkd3d_shader_signature_element *signature_element;
            MTLVertexAttributeDescriptor *attribute;

            signature_element = vkd3d_shader_find_signature_element(&runner->signatures[SHADER_TYPE_VS].input,
                    element->name, element->index, 0);
            ok(signature_element, "Cannot find signature element %s%u.\n", element->name, element->index);

            attribute = [vertex_desc.attributes objectAtIndexedSubscript:signature_element->register_index];
            attribute.bufferIndex = vb_info[element->slot].idx;
            attribute.format = get_metal_attribute_format(element->format);
            ok(attribute.format != MTLVertexFormatInvalid, "Unhandled attribute format %#x.\n", element->format);
            attribute.offset = attribute_offsets[i];
        }
        for (i = 0; i < ARRAY_SIZE(vb_info); ++i)
        {
            if (!vb_info[i].buffer)
                continue;
            [encoder setVertexBuffer:vb_info[i].buffer offset:0 atIndex:vb_info[i].idx];
        }

        pipeline_desc.vertexDescriptor = vertex_desc;

        if (!(pso = [[device newRenderPipelineStateWithDescriptor:pipeline_desc error:&err] autorelease]))
        {
            trace("Failed to compile pipeline state.\n");
            if (err)
                trace_messages([err.localizedDescription UTF8String]);
            [encoder endEncoding];
            goto done;
        }

        if (ds_state)
            [encoder setDepthStencilState:ds_state];

        [encoder setRenderPipelineState:pso];
        [encoder setViewport:viewport];
        [encoder drawPrimitives:get_metal_primitive_type(topology)
                vertexStart:first_vertex
                vertexCount:vertex_count
                instanceCount:instance_count
                baseInstance:first_instance];
        [encoder endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        ret = true;
    }

done:
    for (i = 0; i < SHADER_TYPE_COUNT; ++i)
    {
        if (!runner->d3d_blobs[i])
            continue;

        vkd3d_shader_free_scan_signature_info(&runner->signatures[i]);
        ID3D10Blob_Release(runner->d3d_blobs[i]);
        runner->d3d_blobs[i] = NULL;
    }

    return ret;
}

static bool metal_runner_copy(struct shader_runner *r, struct resource *src, struct resource *dst)
{
    struct metal_resource *s = metal_resource(src);
    struct metal_resource *d = metal_resource(dst);
    struct metal_runner *runner = metal_runner(r);
    id<MTLCommandBuffer> command_buffer;
    id<MTLBlitCommandEncoder> blit;

    if (src->desc.dimension == RESOURCE_DIMENSION_BUFFER)
        return false;

    @autoreleasepool
    {
        command_buffer = [runner->queue commandBuffer];

        blit = [command_buffer blitCommandEncoder];
        [blit copyFromTexture:s->texture toTexture:d->texture];
        [blit endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
    }

    return true;
}

static struct resource_readback *metal_runner_get_resource_readback(struct shader_runner *r,
        struct resource *res, unsigned int sub_resource_idx)
{
    struct metal_resource *resource = metal_resource(res);
    MTLRenderPassColorAttachmentDescriptor *attachment;
    struct metal_runner *runner = metal_runner(r);
    id<MTLRenderCommandEncoder> resolve;
    id<MTLCommandBuffer> command_buffer;
    struct metal_resource_readback *rb;
    MTLRenderPassDescriptor *pass_desc;
    MTLTextureDescriptor *texture_desc;
    id<MTLBlitCommandEncoder> blit;
    id<MTLTexture> src_texture;
    unsigned int layer, level;

    if (resource->r.desc.dimension != RESOURCE_DIMENSION_BUFFER
            && resource->r.desc.dimension != RESOURCE_DIMENSION_2D)
        fatal_error("Unhandled resource dimension %#x.\n", resource->r.desc.dimension);

    rb = malloc(sizeof(*rb));
    rb->rb.width = resource->r.desc.width;
    rb->rb.height = resource->r.desc.height;
    rb->rb.depth = resource->r.desc.depth;
    rb->rb.row_pitch = rb->rb.width * resource->r.desc.texel_size;
    rb->buffer = [runner->device newBufferWithLength:rb->rb.row_pitch * rb->rb.height
            options:DEFAULT_BUFFER_RESOURCE_OPTIONS | MTLResourceStorageModeManaged];

    level = sub_resource_idx % resource->r.desc.level_count;
    layer = sub_resource_idx / resource->r.desc.level_count;

    @autoreleasepool
    {
        command_buffer = [runner->queue commandBuffer];

        src_texture = resource->texture;
        if (resource->r.desc.sample_count > 1)
        {
            pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
            attachment = pass_desc.colorAttachments[0];

            if (resource->r.desc.type != RESOURCE_TYPE_RENDER_TARGET)
                fatal_error("Unhandled multi-sample resolve of resource with type %#x.\n", resource->r.desc.type);

            texture_desc = [[MTLTextureDescriptor new] autorelease];
            texture_desc.textureType = MTLTextureType2D;
            texture_desc.pixelFormat = get_metal_pixel_format(resource->r.desc.format);
            texture_desc.width = resource->r.desc.width;
            texture_desc.height = resource->r.desc.height;
            texture_desc.arrayLength = resource->r.desc.depth;
            texture_desc.mipmapLevelCount = resource->r.desc.level_count;
            texture_desc.sampleCount = 1;
            texture_desc.storageMode = MTLStorageModePrivate;
            texture_desc.usage = MTLTextureUsageRenderTarget;

            src_texture = [[runner->device newTextureWithDescriptor:texture_desc] autorelease];
            ok(src_texture, "Failed to create resolve texture.\n");

            attachment.texture = resource->texture;
            attachment.resolveTexture = src_texture;
            attachment.loadAction = MTLLoadActionLoad;
            attachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;

            resolve = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];
            [resolve endEncoding];
        }

        blit = [command_buffer blitCommandEncoder];
        [blit copyFromTexture:src_texture
                sourceSlice:layer
                sourceLevel:level
                sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(rb->rb.width, rb->rb.height, rb->rb.depth)
                toBuffer:rb->buffer
                destinationOffset:0
                destinationBytesPerRow:rb->rb.row_pitch
                destinationBytesPerImage:0];
        [blit synchronizeResource:rb->buffer];
        [blit endEncoding];

        [command_buffer commit];
        [command_buffer waitUntilCompleted];
    }
    rb->rb.data = rb->buffer.contents;

    return &rb->rb;
}

static void metal_runner_release_readback(struct shader_runner *r, struct resource_readback *rb)
{
    struct metal_resource_readback *metal_rb = CONTAINING_RECORD(rb, struct metal_resource_readback, rb);

    [metal_rb->buffer release];
    free(rb);
}

static const struct shader_runner_ops metal_runner_ops =
{
    .create_resource = metal_runner_create_resource,
    .destroy_resource = metal_runner_destroy_resource,
    .dispatch = metal_runner_dispatch,
    .clear = metal_runner_clear,
    .draw = metal_runner_draw,
    .copy = metal_runner_copy,
    .get_resource_readback = metal_runner_get_resource_readback,
    .release_readback = metal_runner_release_readback,
};

static bool check_msl_support(void)
{
    const enum vkd3d_shader_target_type *target_types;
    unsigned int count, i;

    target_types = vkd3d_shader_get_supported_target_types(VKD3D_SHADER_SOURCE_DXBC_TPF, &count);
    for (i = 0; i < count; ++i)
    {
        if (target_types[i] == VKD3D_SHADER_TARGET_MSL)
            return true;
    }

    return false;
}

static bool check_argument_buffer_support(id<MTLDevice> device)
{
    MTLArgumentDescriptor *d;

    d = [MTLArgumentDescriptor argumentDescriptor];
    d.dataType = MTLDataTypePointer;

    @try
    {
        [[device newArgumentEncoderWithArguments:@[d]] release];
        return true;
    }
    @catch (NSException *e)
    {
        return false;
    }
}

static bool metal_runner_init(struct metal_runner *runner)
{
    NSArray<id<MTLDevice>> *devices;
    id<MTLDevice> device;

    if (!check_msl_support())
    {
        skip("MSL support is not enabled. If this is unintentional, "
                "add -DVKD3D_SHADER_UNSUPPORTED_MSL to CPPFLAGS.\n");
        return false;
    }

    memset(runner, 0, sizeof(*runner));

    devices = MTLCopyAllDevices();
    for (device in devices)
    {
        if (!check_argument_buffer_support(device))
        {
            trace("Ignoring device \"%s\" because it doesn't have usable argument buffer support.\n",
                    [[device name] UTF8String]);
            continue;
        }

        if (!runner->device
                || (!device.lowPower && runner->device.lowPower)
                || (!device.removable && runner->device.removable))
            runner->device = device;
    }
    device = [runner->device retain];
    [devices release];

    if (!device)
    {
        skip("Failed to find a suitable Metal device.\n");
        return false;
    }

    trace("GPU: %s\n", [[device name] UTF8String]);

    if (!(runner->queue = [device newCommandQueue]))
    {
        skip("Failed to create command queue.\n");
        [device release];
        return false;
    }

    runner->caps.runner = "Metal";
    runner->caps.shader_caps[SHADER_CAP_DESCRIPTORS] = true;

    runner->caps.tags[SHADER_RUNNER_TAG_MSL].present = true;

    if (runner->device.readWriteTextureSupport >= MTLReadWriteTextureTier1)
    {
        runner->caps.format_caps[DXGI_FORMAT_R32_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R32_SINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R32_FLOAT] |= FORMAT_CAP_UAV_LOAD;
    }

    if (runner->device.readWriteTextureSupport >= MTLReadWriteTextureTier2)
    {
        runner->caps.format_caps[DXGI_FORMAT_R32G32B32A32_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R32G32B32A32_SINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R32G32B32A32_FLOAT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16G16B16A16_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16G16B16A16_SINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16G16B16A16_FLOAT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8G8B8A8_UNORM] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8G8B8A8_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8G8B8A8_SINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16_SINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R16_FLOAT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8_UNORM] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8_UINT] |= FORMAT_CAP_UAV_LOAD;
        runner->caps.format_caps[DXGI_FORMAT_R8_SINT] |= FORMAT_CAP_UAV_LOAD;
    }

    return true;
}

static void metal_runner_cleanup(struct metal_runner *runner)
{
    [runner->queue release];
    [runner->device release];
}

void run_shader_tests_metal(void *dxc_compiler)
{
    bool skip_sm4 = test_skipping_execution("Metal", HLSL_COMPILER, SHADER_MODEL_4_0, SHADER_MODEL_5_1);
    bool skip_sm6 = test_skipping_execution("Metal", "dxcompiler", SHADER_MODEL_6_0, SHADER_MODEL_MAX);
    struct metal_runner runner;

    if (skip_sm4 && skip_sm6)
        return;

    if (!metal_runner_init(&runner))
        return;

    if (!skip_sm4)
    {
        runner.caps.compiler = HLSL_COMPILER;
        runner.caps.minimum_shader_model = SHADER_MODEL_4_0;
        runner.caps.maximum_shader_model = SHADER_MODEL_5_1;
        run_shader_tests(&runner.r, &runner.caps, &metal_runner_ops, NULL);
    }

    if (dxc_compiler && !skip_sm6)
    {
        runner.caps.compiler = "dxcompiler";
        runner.caps.minimum_shader_model = SHADER_MODEL_6_0;
        runner.caps.maximum_shader_model = SHADER_MODEL_MAX;
        run_shader_tests(&runner.r, &runner.caps, &metal_runner_ops, dxc_compiler);
    }

    metal_runner_cleanup(&runner);
}
