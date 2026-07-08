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

#include <float.h>
#include <stdint.h>
#include "vkd3d_windows.h"
#include "vkd3d_d3dcommon.h"
#include "vkd3d_d3d12.h"
#include "vkd3d_dxgiformat.h"
#include "vkd3d_common.h"
#include "vkd3d_shader.h"
#include "utils.h"

#ifdef VKD3D_CROSSTEST
static const char HLSL_COMPILER[] = "d3dcompiler47.dll";
#else
static const char HLSL_COMPILER[] = "vkd3d-shader";
#endif

#define RENDER_TARGET_WIDTH 640
#define RENDER_TARGET_HEIGHT 480

enum shader_model
{
    SHADER_MODEL_2_0,
    SHADER_MODEL_MIN = SHADER_MODEL_2_0,
    SHADER_MODEL_3_0,
    SHADER_MODEL_4_0,
    SHADER_MODEL_4_1,
    SHADER_MODEL_5_0,
    SHADER_MODEL_5_1,
    SHADER_MODEL_6_0,
    SHADER_MODEL_6_2,
    SHADER_MODEL_6_6,
    SHADER_MODEL_MAX = SHADER_MODEL_6_6,
};

enum shader_type
{
    SHADER_TYPE_CS,
    SHADER_TYPE_PS,
    SHADER_TYPE_VS,
    SHADER_TYPE_HS,
    SHADER_TYPE_DS,
    SHADER_TYPE_GS,
    SHADER_TYPE_FX,
    SHADER_TYPE_COUNT,
};

const char *shader_type_string(enum shader_type type);

struct sampler
{
    unsigned int slot;

    D3D12_FILTER filter;
    D3D12_TEXTURE_ADDRESS_MODE u_address, v_address, w_address;
    D3D12_COMPARISON_FUNC func;
};

enum resource_type
{
    RESOURCE_TYPE_RENDER_TARGET,
    RESOURCE_TYPE_DEPTH_STENCIL,
    RESOURCE_TYPE_TEXTURE,
    RESOURCE_TYPE_UAV,
    RESOURCE_TYPE_VERTEX_BUFFER,
    RESOURCE_TYPE_UNIFORM_BUFFER,
};

enum resource_dimension
{
    RESOURCE_DIMENSION_BUFFER,
    RESOURCE_DIMENSION_2D,
    RESOURCE_DIMENSION_3D,
    RESOURCE_DIMENSION_CUBE,
};

struct resource_desc
{
    unsigned int slot;
    enum resource_type type;
    enum resource_dimension dimension;

    DXGI_FORMAT format;
    unsigned int texel_size;
    unsigned int width, height, depth;
    unsigned int layer_count, level_count;
    unsigned int sample_count;
    float min_lod_clamp;
};

struct resource_params
{
    struct resource_desc desc;

    bool is_shadow;
    bool is_raw;
    bool is_uav_counter;
    bool explicit_format;
    unsigned int stride;

    uint8_t *data;
    size_t data_size, data_capacity;
};

struct resource
{
    struct resource_desc desc;
    bool is_raw;
    bool is_uav_counter;
    unsigned int stride;
};

struct input_element
{
    const char *name;
    unsigned int slot;
    DXGI_FORMAT format;
    unsigned int texel_size;
    unsigned int index;
};

struct viewport
{
    float x;
    float y;
    float width;
    float height;
};

#define MAX_UNIFORM_BUFFERS 2
#define MAX_INPUT_BINDINGS 32
#define MAX_DESCRIPTORS 32
#define MAX_RESOURCES 32
#define MAX_SAMPLERS 32
#define DXGI_FORMAT_COUNT (DXGI_FORMAT_B4G4R4A4_UNORM + 1)

enum format_cap
{
    FORMAT_CAP_UAV_LOAD = 0x00000001,
};

enum shader_cap
{
    SHADER_CAP_CLIP_PLANES,
    SHADER_CAP_CULL_DISTANCE,
    SHADER_CAP_DEPTH_BOUNDS,
    SHADER_CAP_DESCRIPTORS,
    SHADER_CAP_FILTER_MINMAX,
    SHADER_CAP_FLOAT64,
    SHADER_CAP_FOG,
    SHADER_CAP_GEOMETRY_SHADER,
    SHADER_CAP_INT64,
    SHADER_CAP_MIN_LOD_CLAMP,
    SHADER_CAP_NATIVE_16_BIT,
    SHADER_CAP_POINT_SIZE,
    SHADER_CAP_ROV,
    SHADER_CAP_RT_VP_ARRAY_INDEX,
    SHADER_CAP_TESSELLATION_SHADER,
    SHADER_CAP_WAVE_OPS,
    SHADER_CAP_COUNT,
};

enum shader_runner_tag
{
    SHADER_RUNNER_TAG_AMD,
    SHADER_RUNNER_TAG_D3D11,
    SHADER_RUNNER_TAG_D3D12,
    SHADER_RUNNER_TAG_GLSL,
    SHADER_RUNNER_TAG_INTEL,
    SHADER_RUNNER_TAG_LLVM,
    SHADER_RUNNER_TAG_LLVMPIPE,
    SHADER_RUNNER_TAG_MESA,
    SHADER_RUNNER_TAG_MSL,
    SHADER_RUNNER_TAG_MVK,
    SHADER_RUNNER_TAG_NVIDIA,
    SHADER_RUNNER_TAG_OPENGL,
    SHADER_RUNNER_TAG_RADV,
    SHADER_RUNNER_TAG_VULKAN,
    SHADER_RUNNER_TAG_WARP,
    SHADER_RUNNER_TAG_WINDOWS,
    SHADER_RUNNER_TAG_COUNT,
};

struct shader_runner_tag_value
{
    bool present;
    uint32_t major, minor, patch;
};

struct shader_runner_caps
{
    const char *runner;
    const char *compiler;
    struct shader_runner_tag_value tags[SHADER_RUNNER_TAG_COUNT];
    enum shader_model minimum_shader_model;
    enum shader_model maximum_shader_model;
    bool shader_caps[SHADER_CAP_COUNT];

    uint32_t format_caps[DXGI_FORMAT_COUNT];
};

static inline unsigned int shader_runner_caps_get_feature_flags(const struct shader_runner_caps *caps)
{
    unsigned int flags = 0;

    if (caps->shader_caps[SHADER_CAP_INT64])
        flags |= VKD3D_SHADER_COMPILE_OPTION_FEATURE_INT64;
    if (caps->shader_caps[SHADER_CAP_FLOAT64])
        flags |= VKD3D_SHADER_COMPILE_OPTION_FEATURE_FLOAT64;
    if (caps->shader_caps[SHADER_CAP_WAVE_OPS])
        flags |= VKD3D_SHADER_COMPILE_OPTION_FEATURE_WAVE_OPS;

    return flags;
}

enum fog_mode
{
    FOG_MODE_NONE = 0,
    FOG_MODE_EXP = 1,
    FOG_MODE_EXP2 = 2,
    FOG_MODE_LINEAR = 3,
    FOG_MODE_DISABLE,
};

enum source_format
{
    SOURCE_FORMAT_HLSL,
    SOURCE_FORMAT_D3DBC_HEX,
    SOURCE_FORMAT_DXBC_TPF_HEX,
    SOURCE_FORMAT_DXBC_DXIL_HEX,
};

struct descriptor_mapping
{
    enum vkd3d_shader_descriptor_type type;
    unsigned int register_idx;
    unsigned int register_space;
    unsigned int count;
    unsigned int target_idx;
};

struct uniform_buffer
{
    uint32_t *elements;
    size_t count, capacity;
};

struct shader_runner
{
    const struct shader_runner_ops *ops;
    const struct shader_runner_caps *caps;

    bool is_todo;
    bool is_bug;
    bool hlsl_todo[SHADER_MODEL_MAX + 1];
    HRESULT hlsl_hrs[SHADER_MODEL_MAX + 1];

    char *shader_source[SHADER_TYPE_COUNT];
    enum source_format shader_format[SHADER_TYPE_COUNT];
    enum shader_model minimum_shader_model;
    enum shader_model maximum_shader_model;
    enum shader_model shader_model;
    uint32_t test_model_mask;
    bool require_shader_caps[SHADER_CAP_COUNT];
    uint32_t require_format_caps[DXGI_FORMAT_COUNT];
    bool skip_directives;

    bool last_render_failed;

    uint32_t sample_mask;

    struct uniform_buffer uniform_buffers[MAX_UNIFORM_BUFFERS];

    struct resource *resources[MAX_RESOURCES];
    size_t resource_count;
    uint32_t failed_resources[RESOURCE_TYPE_VERTEX_BUFFER + 1][VKD3D_BITMAP_SIZE(MAX_RESOURCES)];
    unsigned int failed_resource_count;

    uint32_t sample_count;

    struct sampler samplers[MAX_SAMPLERS];
    size_t sampler_count;

    struct input_layout
    {
        struct input_element *elements;
        size_t element_count, element_capacity;
    } input;

    IDxcCompiler3 *dxc_compiler;

    unsigned int compile_options;

    D3D12_COMPARISON_FUNC depth_func;
    bool depth_bounds;
    float depth_min, depth_max;

    enum vkd3d_shader_comparison_func alpha_test_func;
    float alpha_test_ref;
    bool flat_shading;
    uint8_t clip_plane_mask;
    struct vec4 clip_planes[8];
    float point_size, point_size_min, point_size_max;
    bool point_sprite;
    struct vec4 fog_colour;
    enum fog_mode fog_mode;
    float fog_start, fog_end, fog_density;
    bool ortho_fog;
    struct bump_constants
    {
        float matrix[2][2];
        float luminance_scale, luminance_offset;
    } bump[8];
    uint8_t projected_texture_mask;

    enum denorm_mode denorm_mode;

    struct viewport viewports[4];
    unsigned int viewport_count;

    struct descriptor_mapping descriptors[MAX_DESCRIPTORS];
    unsigned int descriptor_count;
    bool default_descriptors;
};

struct shader_runner_ops
{
    struct resource *(*create_resource)(struct shader_runner *runner, const struct resource_params *params);
    void (*destroy_resource)(struct shader_runner *runner, struct resource *resource);
    void (*clear)(struct shader_runner *runner, struct resource *resource, const struct vec4 *clear_value);
    bool (*draw)(struct shader_runner *runner, D3D_PRIMITIVE_TOPOLOGY primitive_topology, unsigned int vertex_count,
            unsigned int instance_count, unsigned int first_vertex, unsigned int first_instance);
    bool (*copy)(struct shader_runner *runner, struct resource *src, struct resource *dst);
    bool (*dispatch)(struct shader_runner *runner, unsigned int x, unsigned int y, unsigned int z);
    struct resource_readback *(*get_resource_readback)(struct shader_runner *runner,
            struct resource *resource, unsigned int sub_resource_idx);
    void (*release_readback)(struct shader_runner *runner, struct resource_readback *rb);
};

static inline unsigned int get_level_dimension(unsigned int dimension, unsigned int level)
{
    return max(1, dimension >> level);
}

void fatal_error(const char *format, ...) VKD3D_NORETURN VKD3D_PRINTF_FUNC(1, 2);

unsigned int get_vb_stride(const struct shader_runner *runner, unsigned int slot);
void init_resource(struct resource *resource, const struct resource_params *params);
ID3D10Blob *compile_hlsl(const struct shader_runner *runner, enum shader_type type);
struct sampler *shader_runner_get_sampler(struct shader_runner *runner, unsigned int slot);
struct resource *shader_runner_get_resource(struct shader_runner *runner, enum resource_type type, unsigned int slot);

bool test_skipping_execution(const char *executor, const char *compiler,
    enum shader_model minimum_shader_model, enum shader_model maximum_shader_model);
void run_shader_tests(struct shader_runner *runner, const struct shader_runner_caps *caps,
        const struct shader_runner_ops *ops, void *dxc_compiler);

#ifdef _WIN32
void run_shader_tests_d3d9(void);
void run_shader_tests_d3d11(void);
#else
void run_shader_tests_gl(void);
void run_shader_tests_metal(void *dxc_compiler);
void run_shader_tests_vulkan(void *dxc_compiler);
#endif
void run_shader_tests_d3d12(void *dxc_compiler);
