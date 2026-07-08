/*
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

/*
 * This application contains code derived from piglit, the license for which
 * follows:
 *
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef __MINGW32__
# define _HRESULT_DEFINED
typedef int HRESULT;
#else
# define WIDL_C_INLINE_WRAPPERS
#endif

#define COBJMACROS
#define CONST_VTABLE
#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "vkd3d_windows.h"
#include "vkd3d_d3dcommon.h"
#include "vkd3d_d3dcompiler.h"
#include "vkd3d_test.h"
#include "shader_runner.h"

struct literal
{
    enum
    {
        DATA_F32,
        DATA_F64,
        DATA_I32,
        DATA_I64,
        DATA_U32,
        DATA_U64,
    } data_type;
    unsigned int element_count;
    unsigned int u32_count;
    union
    {
        struct dvec2 f64;
        struct vec4 f32;
        struct i64vec2 i64;
        struct ivec4 i32;
        struct u64vec2 u64;
        struct uvec4 u32;
    } v;
};

struct test_options test_options = {0};

static struct shader_test_options
{
    const char *filename;
    enum shader_model minimum_shader_model, maximum_shader_model;
    const char *executor_filter;
    const char *compiler_filter;
} shader_test_options = {0};

static const char *const sm_strings_dot[] =
{
    [SHADER_MODEL_2_0] = "2.0",
    [SHADER_MODEL_3_0] = "3.0",
    [SHADER_MODEL_4_0] = "4.0",
    [SHADER_MODEL_4_1] = "4.1",
    [SHADER_MODEL_5_0] = "5.0",
    [SHADER_MODEL_5_1] = "5.1",
    [SHADER_MODEL_6_0] = "6.0",
    [SHADER_MODEL_6_2] = "6.2",
    [SHADER_MODEL_6_6] = "6.6",
};

static const char *const sm_strings_underscore[] =
{
    [SHADER_MODEL_2_0] = "2_0",
    [SHADER_MODEL_3_0] = "3_0",
    [SHADER_MODEL_4_0] = "4_0",
    [SHADER_MODEL_4_1] = "4_1",
    [SHADER_MODEL_5_0] = "5_0",
    [SHADER_MODEL_5_1] = "5_1",
    [SHADER_MODEL_6_0] = "6_0",
    [SHADER_MODEL_6_2] = "6_2",
    [SHADER_MODEL_6_6] = "6_6",
};

void fatal_error(const char *format, ...)
{
    unsigned int i;
    va_list args;

    for (i = 0; i < vkd3d_test_state.context_count; ++i)
        fprintf(stderr, "%s: ", vkd3d_test_state.context[i]);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

static void input_layout_clear(struct input_layout *l)
{
    size_t i;

    for (i = 0; i < l->element_count; ++i)
    {
        free((char *)l->elements[i].name);
    }
    l->element_count = 0;
}

static void input_layout_cleanup(struct input_layout *l)
{
    input_layout_clear(l);
    free(l->elements);
}

enum parse_state
{
    STATE_NONE,
    STATE_DESCRIPTORS,
    STATE_HEADER,
    STATE_INPUT_LAYOUT,
    STATE_PREPROC,
    STATE_PREPROC_INVALID,
    STATE_REQUIRE,
    STATE_RESOURCE,
    STATE_SAMPLER,
    STATE_SHADER,
    STATE_TEST,
};

static enum shader_model match_shader_model_string(const char *string, const char **rest)
{
    for (enum shader_model i = 0; i < ARRAY_SIZE(sm_strings_dot); ++i)
    {
        if (!strncmp(string, sm_strings_dot[i], strlen(sm_strings_dot[i])))
        {
            *rest = string + strlen(sm_strings_dot[i]);
            return i;
        }
        /* Allow e.g. "4" as a shorthand for "4.0". */
        if (string[0] == sm_strings_dot[i][0] && !strcmp(&sm_strings_dot[i][1], ".0")
                && string[1] != '.' && !isdigit(string[1]))
        {
            *rest = string + 1;
            return i;
        }
    }

    fatal_error("Unrecognized shader model '%s'.\n", string);
}

static const char * const shader_runner_tag_names[SHADER_RUNNER_TAG_COUNT] =
{
    [SHADER_RUNNER_TAG_AMD]      = "amd",
    [SHADER_RUNNER_TAG_D3D11]    = "d3d11",
    [SHADER_RUNNER_TAG_D3D12]    = "d3d12",
    [SHADER_RUNNER_TAG_GLSL]     = "glsl",
    [SHADER_RUNNER_TAG_INTEL]    = "intel",
    [SHADER_RUNNER_TAG_LLVM]     = "llvm",
    [SHADER_RUNNER_TAG_LLVMPIPE] = "llvmpipe",
    [SHADER_RUNNER_TAG_MESA]     = "mesa",
    [SHADER_RUNNER_TAG_MSL]      = "msl",
    [SHADER_RUNNER_TAG_MVK]      = "mvk",
    [SHADER_RUNNER_TAG_NVIDIA]   = "nvidia",
    [SHADER_RUNNER_TAG_OPENGL]   = "opengl",
    [SHADER_RUNNER_TAG_RADV]     = "radv",
    [SHADER_RUNNER_TAG_VULKAN]   = "vulkan",
    [SHADER_RUNNER_TAG_WARP]     = "warp",
    [SHADER_RUNNER_TAG_WINDOWS]  = "windows",
};

static bool match_tag(struct shader_runner *runner, enum shader_runner_tag tag, const char **line)
{
    int ret, advance_major, advance_minor, advance_patch, comparison_result;
    const struct shader_runner_tag_value *value = &runner->caps->tags[tag];
    uint32_t major, minor, patch;
    char comparison[2];

    comparison[0] = **line;
    switch (comparison[0])
    {
        case '<':
        case '>':
        case '=':
        case '!':
            ++*line;
            break;

        default:
            return value->present;
    }

    comparison[1] = **line;
    switch (comparison[1])
    {
        case '=':
            if (comparison[0] == '=')
                fatal_error("Invalid comparison with '==': '%s'.\n", *line);
            ++*line;
            break;

        default:
            comparison[1] = '\0';
            break;
    }

    if (comparison[0] == '!' && comparison[1] != '=')
        fatal_error("Invalid comparison with '%c%c': '%s'.\n", comparison[0], comparison[1], *line);

    ret = sscanf(*line, "%u%n.%u%n.%u%n", &major, &advance_major,
            &minor, &advance_minor, &patch, &advance_patch);

    switch (ret)
    {
        case 1:
            /* Comparison with just major. */
            *line += advance_major;
            minor = 0;
            patch = 0;
            break;

        case 2:
            /* Comparison with major.minor. */
            *line += advance_minor;
            patch = 0;
            break;

        case 3:
            /* Comparison with major.minor.patch. */
            *line += advance_patch;
            break;

        default:
            fatal_error("Invalid comparison version '%s'.\n", *line);
            break;
    }

    if (!value->present)
        return false;
    comparison_result = compare_versions(value->major, value->minor, value->patch, major, minor, patch);

    switch (comparison[0])
    {
        case '<':
            return comparison[1] == '=' ? comparison_result <= 0 : comparison_result < 0;

        case '>':
            return comparison[1] == '=' ? comparison_result >= 0 : comparison_result > 0;

        case '=':
            return comparison_result == 0;

        case '!':
            return comparison_result != 0;

        default:
            fatal_error("Invalid tag comparison '%s'.\n", *line);
    }
}

static bool match_shader_runner_tag(struct shader_runner *runner,
        const char *line, const char **const rest, bool *holds)
{
    bool negate = false, match = false;

    if (*line == '!')
    {
        negate = true;
        ++line;
    }

    /* Iterate backwards to avoid matching prefixes of other tags
     * (e.g., matching the "llvm" prefix of "llvmpipe"). */
    for (unsigned int i = ARRAY_SIZE(shader_runner_tag_names) - 1; i != UINT_MAX; --i)
    {
        const char *tag_name = shader_runner_tag_names[i];
        size_t tag_len = strlen(tag_name);
        bool tag_match;

        if (strncmp(line, tag_name, tag_len))
            continue;

        line += tag_len;
        tag_match = match_tag(runner, i, &line);
        *holds &= negate ? !tag_match : tag_match;
        match = true;
        break;
    }

    *rest = line;
    return match;
}

static bool check_required_tags(struct shader_runner *runner, const char *line, bool *has_tag)
{
    *has_tag = false;

    while (*line)
    {
        bool holds = true;

        while (*line && *line != '|')
        {
            while (isspace(*line))
                ++line;

            if (!match_shader_runner_tag(runner, line, &line, &holds))
                return false;

            while (isspace(*line))
                ++line;

            if (*line == '&')
                ++line;
        }

        if (*line == '|')
            ++line;

        *has_tag |= holds;
    }

    return true;
}

static bool check_qualifier_args_conjunction(struct shader_runner *runner,
        const char *line, const char **const rest, uint32_t *model_mask)
{
    bool holds = true;

    *model_mask = ~0u;

    while (*line != ')' && *line != '|')
    {
        enum shader_model model;
        bool match = false;

        while (isspace(*line))
            ++line;

        if (!strncmp(line, "sm>=", 4))
        {
            match = true;
            line += 4;
            model = match_shader_model_string(line, &line);
            *model_mask &= ~((1u << model) - 1);
            if (runner->shader_model < model)
                holds = false;
        }
        else if (!strncmp(line, "sm<", 3))
        {
            match = true;
            line += 3;
            model = match_shader_model_string(line, &line);
            *model_mask &= ((1u << model) - 1);
            if (runner->shader_model >= model)
                holds = false;
        }
        else
        {
            match = match_shader_runner_tag(runner, line, &line, &holds);
        }

        while (isspace(*line))
            ++line;

        if (match && *line == '&')
        {
            ++line;
        }
        else if (*line != ')' && *line != '|')
        {
            fatal_error("Invalid qualifier argument '%s'.\n", line);
        }
    }

    assert(*line == ')' || *line == '|');
    if (rest)
        *rest = line;

    return holds;
}

static bool check_qualifier_args(struct shader_runner *runner,
        const char *line, const char **const rest, uint32_t *model_mask)
{
    bool first = true;
    bool holds = false;

    if (*line != '(')
    {
        *model_mask = ~0u;
        return true;
    }
    ++line;

    *model_mask = 0;

    while (*line != ')')
    {
        uint32_t sub_mask;

        if (!first && *line == '|')
            ++line;
        first = false;

        holds = check_qualifier_args_conjunction(runner, line, &line, &sub_mask) || holds;
        *model_mask |= sub_mask;
    }

    assert(*line == ')');
    if (rest)
        *rest = line + 1;

    return holds;
}

static bool match_string_generic(struct shader_runner *runner, const char *line,
        const char *token, const char **const rest, bool allow_qualifier_args)
{
    size_t len = strlen(token);
    uint32_t model_mask;
    bool holds = true;

    while (isspace(*line))
        ++line;

    if (strncmp(line, token, len) || !(isspace(line[len]) || line[len] == '(' || line[len] == ']'))
        return false;
    line += len;

    if (allow_qualifier_args)
        holds = check_qualifier_args(runner, line, &line, &model_mask);

    if (rest)
    {
        *rest = line;
        while (isspace(**rest))
            ++*rest;
    }
    return holds;
}

static bool match_string_with_args(struct shader_runner *runner,
        const char *line, const char *token, const char **const rest)
{
    return match_string_generic(runner, line, token, rest, true);
}

static bool match_string(const char *line, const char *token, const char **const rest)
{
    return match_string_generic(NULL, line, token, rest, false);
}

static const char *close_parentheses(const char *line)
{
    while (isspace(*line))
        ++line;

    if (*line != ')')
        fatal_error("Malformed probe arguments '%s'.\n", line);

    return line;
}

static const struct format_info
{
    const char *string;
    unsigned int texel_size;
    DXGI_FORMAT format;
    bool is_shadow;
}
format_info[] =
{
    {"r32g32b32a32-float",  16, DXGI_FORMAT_R32G32B32A32_FLOAT},
    {"r32g32b32a32-sint",   16, DXGI_FORMAT_R32G32B32A32_SINT},
    {"r32g32b32a32-uint",   16, DXGI_FORMAT_R32G32B32A32_UINT},
    {"r32g32b32-float",     12, DXGI_FORMAT_R32G32B32_FLOAT},
    {"r32g32-float",         8, DXGI_FORMAT_R32G32_FLOAT},
    {"r32g32-sint",          8, DXGI_FORMAT_R32G32_SINT},
    {"r32g32-uint",          8, DXGI_FORMAT_R32G32_UINT},
    {"r32-float-shadow",     4, DXGI_FORMAT_R32_FLOAT, true},
    {"r32-float",            4, DXGI_FORMAT_R32_FLOAT},
    {"r32-sint",             4, DXGI_FORMAT_R32_SINT},
    {"r32-uint",             4, DXGI_FORMAT_R32_UINT},
    {"r32-typeless",         4, DXGI_FORMAT_R32_TYPELESS},
    {"unknown",              0, DXGI_FORMAT_UNKNOWN},
};

static const char *get_format_name(DXGI_FORMAT f)
{
    const struct format_info *info;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(format_info); ++i)
    {
        info = &format_info[i];
        if (info->format == f && !info->is_shadow)
            return info->string;
    }

    return NULL;
}

static DXGI_FORMAT parse_format(const char *line, unsigned int *texel_size, bool *is_shadow, const char **rest)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(format_info); ++i)
    {
        if (match_string(line, format_info[i].string, rest))
        {
            if (texel_size)
                *texel_size = format_info[i].texel_size;
            if (is_shadow)
                *is_shadow = format_info[i].is_shadow;
            return format_info[i].format;
        }
    }

    fatal_error("Unknown format '%s'.\n", line);
}

static const char *const shader_cap_strings[] =
{
    [SHADER_CAP_CLIP_PLANES]         = "clip-planes",
    [SHADER_CAP_CULL_DISTANCE]       = "cull-distance",
    [SHADER_CAP_DEPTH_BOUNDS]        = "depth-bounds",
    [SHADER_CAP_DESCRIPTORS]         = "descriptors",
    [SHADER_CAP_FILTER_MINMAX]       = "filter-minmax",
    [SHADER_CAP_FLOAT64]             = "float64",
    [SHADER_CAP_FOG]                 = "fog",
    [SHADER_CAP_GEOMETRY_SHADER]     = "geometry-shader",
    [SHADER_CAP_INT64]               = "int64",
    [SHADER_CAP_MIN_LOD_CLAMP]       = "min-lod-clamp",
    [SHADER_CAP_NATIVE_16_BIT]       = "native-16-bit",
    [SHADER_CAP_POINT_SIZE]          = "point-size",
    [SHADER_CAP_ROV]                 = "rov",
    [SHADER_CAP_RT_VP_ARRAY_INDEX]   = "rt-vp-array-index",
    [SHADER_CAP_TESSELLATION_SHADER] = "tessellation-shader",
    [SHADER_CAP_WAVE_OPS]            = "wave-ops",
};

static bool match_shader_cap_string(const char *line, enum shader_cap *cap)
{
    for (enum shader_cap i = 0; i < SHADER_CAP_COUNT; ++i)
    {
        if (match_string(line, shader_cap_strings[i], &line))
        {
            *cap = i;
            return true;
        }
    }
    return false;
}

static void parse_require_directive(struct shader_runner *runner, const char *line)
{
    enum shader_cap shader_cap;
    bool less_than = false;
    bool equal = false;
    bool holds = true;
    unsigned int i;

    if (match_string(line, "shader model >=", &line)
            || (equal = match_string(line, "shader model =", &line))
            || (less_than = match_string(line, "shader model <", &line)))
    {
        for (i = 0; i < ARRAY_SIZE(sm_strings_dot); ++i)
        {
            if (match_string(line, sm_strings_dot[i], &line))
            {
                if (equal)
                {
                    runner->maximum_shader_model = min(runner->maximum_shader_model, i);
                    runner->minimum_shader_model = max(runner->minimum_shader_model, i);
                }
                else if (less_than)
                {
                    if (!i)
                        fatal_error("Shader model < '%s' is invalid.\n", line);
                    runner->maximum_shader_model = min(runner->maximum_shader_model, i - 1);
                }
                else
                {
                    runner->minimum_shader_model = max(runner->minimum_shader_model, i);
                }
                return;
            }
        }

        fatal_error("Unknown shader model '%s'.\n", line);
    }
    else if (match_string(line, "options:", &line))
    {
        static const struct option
        {
            unsigned int option;
            const char *name;
        }
        options[] =
        {
            { 0, "none" },
            { D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, "row-major" },
            { D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, "column-major" },
            { D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, "backcompat" },
            { D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, "unbounded-descriptor-arrays" },
        };

        runner->compile_options = 0;
        for (i = 0; i < ARRAY_SIZE(options); ++i)
        {
            if (match_string(line, options[i].name, &line))
                runner->compile_options |= options[i].option;
        }
    }
    else if (match_string(line, "format", &line))
    {
        DXGI_FORMAT format = parse_format(line, NULL, NULL, &line);

        while (line[0] != '\0')
        {
            if (match_string(line, "uav-load", &line))
                runner->require_format_caps[format] |= FORMAT_CAP_UAV_LOAD;
            else
                fatal_error("Unknown format cap '%s'.\n", line);
        }
    }
    else if (match_shader_cap_string(line, &shader_cap))
    {
        runner->require_shader_caps[shader_cap] = true;
    }
    else if (check_required_tags(runner, line, &holds))
    {
        runner->skip_directives |= !holds;
    }
    else
    {
        fatal_error("Unknown require directive '%s'.\n", line);
    }
}

static D3D12_COMPARISON_FUNC parse_comparison_func(const char *line, const char **rest)
{
    static const struct
    {
        const char *string;
        D3D12_COMPARISON_FUNC func;
    }
    funcs[] =
    {
        {"less equal", D3D12_COMPARISON_FUNC_LESS_EQUAL},
        {"not equal", D3D12_COMPARISON_FUNC_NOT_EQUAL},
        {"greater equal", D3D12_COMPARISON_FUNC_GREATER_EQUAL},
        {"never", D3D12_COMPARISON_FUNC_NEVER},
        {"less", D3D12_COMPARISON_FUNC_LESS},
        {"equal", D3D12_COMPARISON_FUNC_EQUAL},
        {"greater", D3D12_COMPARISON_FUNC_GREATER},
        {"always", D3D12_COMPARISON_FUNC_ALWAYS},
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(funcs); ++i)
    {
        if (match_string(line, funcs[i].string, rest))
            return funcs[i].func;
    }

    fatal_error("Unknown comparison func '%s'.\n", line);
}

static D3D12_TEXTURE_ADDRESS_MODE parse_sampler_address_mode(const char *line, const char **rest)
{
    if (match_string(line, "border", rest))
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    if (match_string(line, "clamp", rest))
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    if (match_string(line, "mirror_once", rest))
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
    if (match_string(line, "mirror", rest))
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    if (match_string(line, "wrap", rest))
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    fatal_error("Unknown sampler address mode '%s'.\n", line);
}

static void parse_sampler_directive(struct sampler *sampler, const char *line)
{
    if (match_string(line, "address", &line))
    {
        sampler->u_address = parse_sampler_address_mode(line, &line);
        sampler->v_address = parse_sampler_address_mode(line, &line);
        sampler->w_address = parse_sampler_address_mode(line, &line);
    }
    else if (match_string(line, "filter", &line))
    {
        D3D12_FILTER_REDUCTION_TYPE reduction = D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        unsigned int i;

        static const struct
        {
            const char *string;
            D3D12_FILTER filter;
        }
        filters[] =
        {
            {"point point point",       D3D12_FILTER_MIN_MAG_MIP_POINT},
            {"point point linear",      D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR},
            {"point linear point",      D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT},
            {"point linear linear",     D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR},
            {"linear point point",      D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT},
            {"linear point linear",     D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR},
            {"linear linear point",     D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT},
            {"linear linear linear",    D3D12_FILTER_MIN_MAG_MIP_LINEAR},
        };

        if (sampler->func)
            reduction = D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
        else if (match_string(line, "minimum", &line))
            reduction = D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
        else if (match_string(line, "maximum", &line))
            reduction = D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;

        for (i = 0; i < ARRAY_SIZE(filters); ++i)
        {
            if (match_string(line, filters[i].string, &line))
            {
                sampler->filter = (reduction << D3D12_FILTER_REDUCTION_TYPE_SHIFT) | filters[i].filter;
                return;
            }
        }

        fatal_error("Unknown sampler filter '%s'.\n", line);
    }
    else if (match_string(line, "comparison", &line))
    {
        sampler->filter &= ~(D3D12_FILTER_REDUCTION_TYPE_MASK << D3D12_FILTER_REDUCTION_TYPE_SHIFT);
        sampler->filter |= D3D12_FILTER_REDUCTION_TYPE_COMPARISON << D3D12_FILTER_REDUCTION_TYPE_SHIFT;
        sampler->func = parse_comparison_func(line, &line);
        return;
    }
    else
    {
        fatal_error("Unknown sampler directive '%s'.\n", line);
    }
}

static void parse_resource_directive(struct resource_params *resource, const char *line)
{
    if (match_string(line, "format", &line))
    {
        resource->desc.format = parse_format(line, &resource->desc.texel_size, &resource->is_shadow, &line);
        assert_that(!resource->explicit_format, "Resource format already specified.\n");
        resource->explicit_format = true;
    }
    else if (match_string(line, "stride", &line))
    {
        if (sscanf(line, "%u", &resource->stride) < 1)
            fatal_error("Malformed texture stride '%s'.\n", line);
        resource->desc.texel_size = resource->stride;
        resource->desc.format = DXGI_FORMAT_UNKNOWN;
        assert_that(!resource->explicit_format, "Resource format already specified.\n");
        resource->explicit_format = true;
    }
    else if (match_string(line, "size", &line))
    {
        resource->desc.height = resource->desc.depth = resource->desc.layer_count = 1;

        if (sscanf(line, "( buffer , %u ) ", &resource->desc.width) == 1)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_BUFFER;
        }
        else if (sscanf(line, "( raw_buffer , %u ) ", &resource->desc.width) == 1)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_BUFFER;
            resource->is_raw = true;
        }
        else if (sscanf(line, "( counter_buffer , %u ) ", &resource->desc.width) == 1)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_BUFFER;
            resource->is_uav_counter = true;
            resource->stride = sizeof(uint32_t);
            resource->desc.texel_size = resource->stride;
            resource->desc.format = DXGI_FORMAT_UNKNOWN;
            assert_that(!resource->explicit_format, "Resource format already specified.\n");
            resource->explicit_format = true;
        }
        else if (sscanf(line, "( 2d , %u , %u ) ", &resource->desc.width, &resource->desc.height) == 2)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_2D;
        }
        else if (sscanf(line, "( 2dms , %u , %u , %u ) ",
                &resource->desc.sample_count, &resource->desc.width, &resource->desc.height) == 3)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_2D;
        }
        else if (sscanf(line, "( 2darray , %u , %u , %u ) ", &resource->desc.width, &resource->desc.height,
                &resource->desc.layer_count) == 3)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_2D;
        }
        else if (sscanf(line, "( 3d , %u , %u , %u ) ", &resource->desc.width, &resource->desc.height,
                &resource->desc.depth) == 3)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_3D;
        }
        else if (sscanf(line, "( cube , %u ) ", &resource->desc.width) == 1)
        {
            resource->desc.dimension = RESOURCE_DIMENSION_CUBE;
            resource->desc.height = resource->desc.width;
            resource->desc.layer_count = 6;
        }
        else
        {
            fatal_error("Malformed resource size '%s'.\n", line);
        }
    }
    else if (match_string(line, "levels", &line))
    {
        char *rest;

        resource->desc.level_count = strtoul(line, &rest, 10);
        if (rest == line)
            fatal_error("Malformed texture directive '%s'.\n", line);
    }
    else if (match_string(line, "min-lod-clamp", &line))
    {
        char *rest;

        resource->desc.min_lod_clamp = strtof(line, &rest);
        if (rest == line)
            fatal_error("Malformed texture directive '%s'.\n", line);
    }
    else
    {
        union
        {
            float f;
            uint32_t u;
        } u;
        char *rest;

        for (;;)
        {
            while (isspace(*line))
                ++line;
            u.u = strtoul(line, &rest, 0);
            if (*line == '.' || (rest && *rest == '.'))
                u.f = strtof(line, &rest);

            if (rest == line)
                break;

            vkd3d_array_reserve((void **)&resource->data, &resource->data_capacity, resource->data_size + sizeof(u), 1);
            memcpy(resource->data + resource->data_size, &u, sizeof(u));
            resource->data_size += sizeof(u);
            line = rest;
        }
    }
}

static void parse_input_layout_directive(struct shader_runner *runner, const char *line)
{
    struct input_element *element;
    const char *rest;
    char *name;

    vkd3d_array_reserve((void **)&runner->input.elements, &runner->input.element_capacity,
            runner->input.element_count + 1, sizeof(*runner->input.elements));
    element = &runner->input.elements[runner->input.element_count++];

    element->slot = strtoul(line, (char **)&rest, 10);
    if (rest == line)
        fatal_error("Malformed input layout directive '%s'.\n", line);
    line = rest;

    element->format = parse_format(line, &element->texel_size, NULL, &line);

    if (!(rest = strpbrk(line, " \n")))
        rest = line + strlen(line);
    name = malloc(rest - line + 1);
    memcpy(name, line, rest - line);
    name[rest - line] = 0;
    element->name = name;
    line = rest;

    element->index = strtoul(line, (char **)&rest, 10);
    if (rest == line)
        element->index = 0;
}

void init_resource(struct resource *resource, const struct resource_params *params)
{
    resource->desc = params->desc;
    resource->is_raw = params->is_raw;
    resource->is_uav_counter = params->is_uav_counter;
    resource->stride = params->stride;
}

struct resource *shader_runner_get_resource(struct shader_runner *runner, enum resource_type type, unsigned int slot)
{
    struct resource *resource;
    size_t i;

    for (i = 0; i < runner->resource_count; ++i)
    {
        resource = runner->resources[i];

        if (resource->desc.type == type && resource->desc.slot == slot)
            return resource;
    }

    return NULL;
}

static void set_resource(struct shader_runner *runner, const struct resource_params *params)
{
    struct resource *resource;
    size_t i;

    if (!(resource = runner->ops->create_resource(runner, params)))
    {
        if (!bitmap_is_set(runner->failed_resources[params->desc.type], params->desc.slot))
        {
            ++runner->failed_resource_count;
            bitmap_set(runner->failed_resources[params->desc.type], params->desc.slot);
        }
        return;
    }

    if (bitmap_is_set(runner->failed_resources[params->desc.type], params->desc.slot))
    {
        assert(runner->failed_resource_count);
        --runner->failed_resource_count;
        bitmap_clear(runner->failed_resources[params->desc.type], params->desc.slot);
    }

    for (i = 0; i < runner->resource_count; ++i)
    {
        if (runner->resources[i]->desc.slot == resource->desc.slot
                && runner->resources[i]->desc.type == resource->desc.type)
        {
            runner->ops->destroy_resource(runner, runner->resources[i]);
            runner->resources[i] = resource;
            return;
        }
    }

    if (runner->resource_count == MAX_RESOURCES)
        fatal_error("Too many resources declared.\n");

    runner->resources[runner->resource_count++] = resource;
}

static void set_default_target(struct shader_runner *runner)
{
    struct resource_params params = {0};

    if (shader_runner_get_resource(runner, RESOURCE_TYPE_RENDER_TARGET, 0)
            || shader_runner_get_resource(runner, RESOURCE_TYPE_DEPTH_STENCIL, 0))
        return;

    params.desc.slot = 0;
    params.desc.type = RESOURCE_TYPE_RENDER_TARGET;
    params.desc.dimension = RESOURCE_DIMENSION_2D;
    params.desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    params.desc.texel_size = 16;
    params.desc.width = RENDER_TARGET_WIDTH;
    params.desc.height = RENDER_TARGET_HEIGHT;
    params.desc.depth = 1;
    params.desc.layer_count = 1;
    params.desc.level_count = 1;

    set_resource(runner, &params);
}

static void uniform_buffer_write(struct uniform_buffer *u, size_t offset, size_t count, const void *data)
{
    size_t initial_count;

    initial_count = u->count;
    u->count = align(max(initial_count, offset + count), 4);
    vkd3d_array_reserve((void **)&u->elements, &u->capacity, u->count, sizeof(*u->elements));
    memset(&u->elements[initial_count], 0x7f, (u->count - initial_count) * sizeof(*u->elements));
    memcpy(&u->elements[offset], data, count * sizeof(*u->elements));
}

static void read_int(const char **line, int *i, bool is_uniform)
{
    char *rest;
    long val;

    errno = 0;
    val = strtol(*line, &rest, 0);

    if (errno != 0 || (is_uniform && *rest != '\0' && !isspace((unsigned char)*rest)))
        fatal_error("Malformed int constant '%s'.\n", *line);

    *i = val;
    if (*i != val)
        fatal_error("Out of range int constant '%.*s'.\n", (int)(rest - *line), *line);

    *line = rest;
}

static void read_uint(const char **line, unsigned int *u, bool is_uniform)
{
    char *rest;
    unsigned long val;

    errno = 0;
    val = strtoul(*line, &rest, 0);

    if (errno != 0 || (is_uniform && *rest != '\0' && !isspace((unsigned char)*rest)))
        fatal_error("Malformed uint constant '%s'.\n", *line);

    *u = val;
    if (*u != val)
        fatal_error("Out of range uint constant '%.*s'.\n", (int)(rest - *line), *line);

    *line = rest;
}

static void read_f32(const char **line, float *f)
{
    char *rest;
    float val;

    errno = 0;
    val = strtof(*line, &rest);

    if (errno != 0 || rest == *line)
        fatal_error("Malformed f32 constant '%s'.\n", *line);

    *f = val;
    *line = rest;
}

static void read_int4(const char **line, struct ivec4 *v)
{
    read_int(line, &v->x, true);
    read_int(line, &v->y, true);
    read_int(line, &v->z, true);
    read_int(line, &v->w, true);
}

static void read_uint4(const char **line, struct uvec4 *v)
{
    read_uint(line, &v->x, true);
    read_uint(line, &v->y, true);
    read_uint(line, &v->z, true);
    read_uint(line, &v->w, true);
}

static void read_int64(const char **line, int64_t *i, bool is_uniform)
{
    char *rest;
    int64_t val;

    errno = 0;
    val = strtoll(*line, &rest, 0);

    if (errno != 0 || (is_uniform && *rest != '\0' && !isspace((unsigned char)*rest)))
        fatal_error("Malformed int64 constant '%s'.\n", *line);

    *i = val;
    *line = rest;
}

static void read_uint64(const char **line, uint64_t *u, bool is_uniform)
{
    char *rest;
    uint64_t val;

    errno = 0;
    val = strtoull(*line, &rest, 0);

    if (errno != 0 || (is_uniform && *rest != '\0' && !isspace((unsigned char)*rest)))
        fatal_error("Malformed uint64 constant '%s'.\n", *line);

    *u = val;
    *line = rest;
}

static void read_f64(const char **line, double *d)
{
    double val;
    char *rest;

    errno = 0;
    val = strtod(*line, &rest);

    if (errno != 0 || rest == *line)
        fatal_error("Malformed f64 constant '%s'.\n", *line);

    *d = val;
    *line = rest;
}

static void read_int64_t2(const char **line, struct i64vec2 *v)
{
    read_int64(line, &v->x, true);
    read_int64(line, &v->y, true);
}

static void read_uint64_t2(const char **line, struct u64vec2 *v)
{
    read_uint64(line, &v->x, true);
    read_uint64(line, &v->y, true);
}

static bool parse_resource_reference(const char **const line, enum resource_type *type, unsigned int *slot)
{
    int len;

    if (match_string(*line, "cbv-f32", line))
    {
        *type = RESOURCE_TYPE_UNIFORM_BUFFER;
        *slot = VKD3D_SHADER_D3DBC_FLOAT_CONSTANT_REGISTER;
        return true;
    }

    if (match_string(*line, "cbv-i32", line))
    {
        *type = RESOURCE_TYPE_UNIFORM_BUFFER;
        *slot = VKD3D_SHADER_D3DBC_INT_CONSTANT_REGISTER;
        return true;
    }

    if (sscanf(*line, " cbv-%u%n", slot, &len) == 1)
    {
        *type = RESOURCE_TYPE_UNIFORM_BUFFER;
        *line += len;
        return true;
    }

    if (match_string(*line, "dsv", line))
    {
        *type = RESOURCE_TYPE_DEPTH_STENCIL;
        *slot = 0;
        return true;
    }

    if (sscanf(*line, " rtv-%u%n", slot, &len) == 1)
    {
        *type = RESOURCE_TYPE_RENDER_TARGET;
        *line += len;
        return true;
    }

    if (sscanf(*line, " srv-%u%n", slot, &len) == 1)
    {
        *type = RESOURCE_TYPE_TEXTURE;
        *line += len;
        return true;
    }

    if (sscanf(*line, " uav-%u%n", slot, &len) == 1)
    {
        *type = RESOURCE_TYPE_UAV;
        *line += len;
        return true;
    }

    if (sscanf(*line, " vb-%u%n", slot, &len) == 1)
    {
        *type = RESOURCE_TYPE_VERTEX_BUFFER;
        *line += len;
        return true;
    }

    return false;
}

static void read_literal(const char **line, struct literal *l)
{
    bool is_signed = false;

    l->element_count = 0;

    if (match_string(*line, "f32", line) || match_string(*line, "rgba", line))
    {
        if (**line != '(')
            fatal_error("Malformed literal '%s'.\n", *line);
        ++*line;

        l->data_type = DATA_F32;
        read_f32(line, &l->v.f32.x);
        ++l->element_count;

        while (isspace(**line))
            ++*line;
        if (**line == ',')
        {
            ++*line;
            read_f32(line, &l->v.f32.y);
            ++l->element_count;

            while (isspace(**line))
                ++*line;
            if (**line == ',')
            {
                ++*line;
                read_f32(line, &l->v.f32.z);
                ++l->element_count;

                while (isspace(**line))
                    ++*line;
                if (**line != ',')
                    fatal_error("Malformed literal '%s'.\n", *line);
                ++*line;
                read_f32(line, &l->v.f32.w);
                ++l->element_count;
            }
        }

        *line = close_parentheses(*line);
        ++*line;
        l->u32_count = l->element_count;

        return;
    }

    if (match_string(*line, "f64", line))
    {
        if (**line != '(')
            fatal_error("Malformed literal '%s'.\n", *line);
        ++*line;

        l->data_type = DATA_F64;
        read_f64(line, &l->v.f64.x);
        ++l->element_count;

        while (isspace(**line))
            ++*line;
        if (**line == ',')
        {
            ++*line;
            read_f64(line, &l->v.f64.y);
            ++l->element_count;
        }

        *line = close_parentheses(*line);
        ++*line;
        l->u32_count = l->element_count * 2;

        return;
    }

    if (match_string(*line, "u32", line) || (is_signed = match_string(*line, "i32", line)))
    {
        if (**line != '(')
            fatal_error("Malformed literal '%s'.\n", *line);
        ++*line;

        l->data_type = is_signed ? DATA_I32 : DATA_U32;
        if (is_signed)
            read_int(line, &l->v.i32.x, false);
        else
            read_uint(line, &l->v.u32.x, false);
        ++l->element_count;

        while (isspace(**line))
            ++*line;
        if (**line == ',')
        {
            ++*line;
            if (is_signed)
                read_int(line, &l->v.i32.y, false);
            else
                read_uint(line, &l->v.u32.y, false);
            ++l->element_count;

            while (isspace(**line))
                ++*line;
            if (**line == ',')
            {
                ++*line;
                if (is_signed)
                    read_int(line, &l->v.i32.z, false);
                else
                    read_uint(line, &l->v.u32.z, false);
                ++l->element_count;

                while (isspace(**line))
                    ++*line;
                if (**line != ',')
                    fatal_error("Malformed literal '%s'.\n", *line);
                ++*line;
                if (is_signed)
                    read_int(line, &l->v.i32.w, false);
                else
                    read_uint(line, &l->v.u32.w, false);
                ++l->element_count;
            }
        }

        *line = close_parentheses(*line);
        ++*line;
        l->u32_count = l->element_count;

        return;
    }

    if (match_string(*line, "u64", line) || (is_signed = match_string(*line, "i64", line)))
    {
        if (**line != '(')
            fatal_error("Malformed literal '%s'.\n", *line);
        ++*line;

        l->data_type = is_signed ? DATA_I64 : DATA_U64;
        if (is_signed)
            read_int64(line, &l->v.i64.x, false);
        else
            read_uint64(line, &l->v.u64.x, false);
        ++l->element_count;

        while (isspace(**line))
            ++*line;
        if (**line == ',')
        {
            ++*line;
            if (is_signed)
                read_int64(line, &l->v.i64.y, false);
            else
                read_uint64(line, &l->v.u64.y, false);
            ++l->element_count;
        }

        *line = close_parentheses(*line);
        ++*line;
        l->u32_count = l->element_count * 2;

        return;
    }

    fatal_error("Malformed literal '%s'.\n", *line);
}

static void parse_test_directive(struct shader_runner *runner, const char *line)
{
    bool skip_directive = false;
    const char *line_ini;
    bool match = true;
    char *rest;
    int ret;

    runner->is_todo = false;
    runner->is_bug = false;

    while (match)
    {
        match = false;

        if (match_string_with_args(runner, line, "todo", &line))
        {
            runner->is_todo = true;
            match = true;
        }

        if (match_string_with_args(runner, line, "bug", &line))
        {
            runner->is_bug = true;
            match = true;
        }

        /* For tests whose results are inconsistent across Windows D3D
         * implementations/drivers, but we want to run to verify that our
         * implementation behaves as we expect. */
        if (match_string_with_args(runner, line, "winbug", &line))
        {
            if (runner->caps->tags[SHADER_RUNNER_TAG_WINDOWS].present)
                skip_directive = true;
            match = true;
        }

        line_ini = line;
        if (match_string_with_args(runner, line, "if", &line))
        {
            match = true;
        }
        else if (line != line_ini)
        {
            /* Matched "if" but for other shader models. */
            skip_directive = true;
            match = true;
        }
    }

    if (skip_directive)
    {
        const char *new_line;

        if ((new_line = strchr(line, '\n')))
            line = new_line + 1;
        else
            line += strlen(line);
        return;
    }

    if (match_string(line, "dispatch", &line))
    {
        unsigned int x, y, z;

        ret = sscanf(line, "%u %u %u", &x, &y, &z);
        if (ret < 3)
            fatal_error("Malformed dispatch arguments '%s'.\n", line);

        runner->last_render_failed = !runner->ops->dispatch(runner, x, y, z);
        todo_if(runner->is_todo) bug_if(runner->is_bug)
        ok(!runner->last_render_failed, "Dispatch failed.\n");
    }
    else if (match_string(line, "clear", &line))
    {
        struct resource *resource;
        enum resource_type type;
        unsigned int slot;
        struct literal l;

        if (!parse_resource_reference(&line, &type, &slot))
            fatal_error("Malformed resource reference '%s'.\n", line);
        if (type == RESOURCE_TYPE_RENDER_TARGET)
            set_default_target(runner);
        if (!(resource = shader_runner_get_resource(runner, type, slot)))
            fatal_error("Undefined resource.\n");
        read_literal(&line, &l);
        if (l.data_type != DATA_F32)
            fatal_error("Unhandled literal data type %#x.\n", l.data_type);

        runner->ops->clear(runner, resource, &l.v.f32);
    }
    else if (match_string(line, "depth-bounds", &line))
    {
        if (sscanf(line, "%f %f", &runner->depth_min, &runner->depth_max) != 2)
            fatal_error("Malformed depth-bounds arguments '%s'.\n", line);
        if (!runner->caps->shader_caps[SHADER_CAP_DEPTH_BOUNDS])
            fatal_error("depth-bounds set but runner does not support depth bounds testing.");
        runner->depth_bounds = true;
    }
    else if (match_string(line, "depth", &line))
    {
        runner->depth_func = parse_comparison_func(line, &line);
    }
    else if (match_string(line, "draw quad", &line))
    {
        struct input_layout input_layout;
        struct resource_params params;
        unsigned int i;

        /* For simplicity, draw a large triangle instead. */
        static const struct vec2 quad[] =
        {
            {-2.0f, -2.0f},
            {-2.0f,  4.0f},
            { 4.0f, -2.0f},
        };

        static const struct input_element position_element =
        {
            .name = "position",
            .slot = 0,
            .format = DXGI_FORMAT_R32G32_FLOAT,
            .texel_size = sizeof(*quad),
            .index = 0,
        };

        static const struct input_layout quad_layout =
        {
            .elements = (struct input_element *)&position_element,
            .element_count = 1,
            .element_capacity = 1,
        };

        static const char vs_source[] =
            "float4 main(float4 pos : position) : sv_position\n"
            "{\n"
            "    return pos;\n"
            "}";

        if (!runner->shader_source[SHADER_TYPE_HS] != !runner->shader_source[SHADER_TYPE_DS])
            fatal_error("Have a domain or hull shader but not both.\n");

        set_default_target(runner);

        input_layout = runner->input;
        runner->input = quad_layout;

        memset(&params, 0, sizeof(params));
        params.desc.slot = 0;
        params.desc.type = RESOURCE_TYPE_VERTEX_BUFFER;
        params.desc.dimension = RESOURCE_DIMENSION_BUFFER;
        params.desc.width = sizeof(quad);
        params.data = malloc(sizeof(quad));
        memcpy(params.data, quad, sizeof(quad));
        params.data_size = sizeof(quad);
        set_resource(runner, &params);
        free(params.data);

        if (!runner->shader_source[SHADER_TYPE_VS])
            runner->shader_source[SHADER_TYPE_VS] = strdup(vs_source);

        runner->sample_count = 1;
        for (i = 0; i < runner->resource_count; ++i)
        {
            if (runner->resources[i]->desc.type == RESOURCE_TYPE_RENDER_TARGET
                    || runner->resources[i]->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
                runner->sample_count = max(runner->sample_count, runner->resources[i]->desc.sample_count);
        }

        runner->last_render_failed = !runner->ops->draw(runner, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, 3, 1, 0, 0);
        todo_if(runner->is_todo) bug_if(runner->is_bug)
        ok(!runner->last_render_failed, "Draw failed.\n");

        runner->input = input_layout;
    }
    else if (match_string(line, "draw", &line))
    {
        unsigned int vertex_count, instance_count, first_vertex, first_instance;
        D3D_PRIMITIVE_TOPOLOGY topology;

        if (!runner->shader_source[SHADER_TYPE_HS] != !runner->shader_source[SHADER_TYPE_DS])
            fatal_error("Have a domain or hull shader but not both.\n");

        set_default_target(runner);

        if (match_string(line, "triangle list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        else if (match_string(line, "triangle strip", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        else if (match_string(line, "point list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        else if (match_string(line, "1 control point patch list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
        else if (match_string(line, "2 control point patch list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;
        else if (match_string(line, "3 control point patch list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
        else if (match_string(line, "4 control point patch list", &line))
            topology = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
        else
            fatal_error("Unknown primitive topology '%s'.\n", line);

        vertex_count = strtoul(line, &rest, 10);
        if (line == rest)
            fatal_error("Malformed vertex count '%s'.\n", line);
        instance_count = strtoul(line = rest, &rest, 10);
        if (line == rest)
            instance_count = 1;
        first_vertex = strtoul(line = rest, &rest, 10);
        if (line == rest)
            first_vertex = 0;
        first_instance = strtoul(line = rest, &rest, 10);
        if (line == rest)
            first_instance = 0;

        runner->sample_count = 1;
        for (unsigned int i = 0; i < runner->resource_count; ++i)
        {
            if (runner->resources[i]->desc.type == RESOURCE_TYPE_RENDER_TARGET
                    || runner->resources[i]->desc.type == RESOURCE_TYPE_DEPTH_STENCIL)
                runner->sample_count = max(runner->sample_count, runner->resources[i]->desc.sample_count);
        }

        runner->last_render_failed = !runner->ops->draw(runner, topology,
                vertex_count, instance_count, first_vertex, first_instance);
        todo_if(runner->is_todo) bug_if(runner->is_bug)
        ok(!runner->last_render_failed, "Draw failed.\n");
    }
    else if (match_string(line, "copy", &line))
    {
        enum resource_type src_type, dst_type;
        unsigned int src_slot, dst_slot;
        struct resource *src, *dst;

        if (!parse_resource_reference(&line, &src_type, &src_slot))
            fatal_error("Malformed resource reference '%s'.\n", line);
        if (!parse_resource_reference(&line, &dst_type, &dst_slot))
            fatal_error("Malformed resource reference '%s'.\n", line);

        if (!(src = shader_runner_get_resource(runner, src_type, src_slot)))
            fatal_error("Undefined source resource.\n");
        if (!(dst = shader_runner_get_resource(runner, dst_type, dst_slot)))
            fatal_error("Undefined destination resource.\n");

        if (src->desc.dimension != dst->desc.dimension
                || src->desc.texel_size != dst->desc.texel_size
                || src->desc.width != dst->desc.width
                || src->desc.height != dst->desc.height
                || src->desc.level_count != dst->desc.level_count
                || src->desc.sample_count != dst->desc.sample_count)
            fatal_error("Resource dimensions don't match.\n");

        ret = runner->ops->copy(runner, src, dst);
        todo_if(runner->is_todo) bug_if(runner->is_bug)
        ok(ret, "Failed to copy resource.\n");
    }
    else if (match_string(line, "probe", &line))
    {
        unsigned int left, top, right, bottom, ulps, slot, array_layer = 0;
        struct resource_readback *rb;
        struct resource *resource;
        enum resource_type type;
        struct literal l;
        D3D12_BOX box;
        RECT rect;
        int len;

        if (runner->last_render_failed)
            return;

        if (!parse_resource_reference(&line, &type, &slot))
        {
            type = RESOURCE_TYPE_RENDER_TARGET;
            slot = 0;
        }

        if (!(resource = shader_runner_get_resource(runner, type, slot)))
            fatal_error("Undefined resource.\n");

        if (sscanf(line, " ( %d , %d , %d , %d )%n", &left, &top, &right, &bottom, &len) == 4)
        {
            set_rect(&rect, left, top, right, bottom);
            line += len;
        }
        else if (sscanf(line, " ( %u , %u , %u )%n", &left, &top, &array_layer, &len) == 3)
        {
            set_rect(&rect, left, top, left + 1, top + 1);
            line += len;
        }
        else if (sscanf(line, " ( %u , %u )%n", &left, &top, &len) == 2)
        {
            set_rect(&rect, left, top, left + 1, top + 1);
            line += len;
        }
        else if (sscanf(line, " ( %u )%n", &left, &len) == 1)
        {
            set_rect(&rect, left, 0, left + 1, 1);
            line += len;
        }
        else
        {
            fatal_error("Malformed probe arguments '%s'.\n", line);
        }

        rb = runner->ops->get_resource_readback(runner, resource, array_layer * resource->desc.level_count);

        box.left = rect.left;
        box.right = rect.right;
        box.top = rect.top;
        box.bottom = rect.bottom;
        box.front = 0;
        box.back = 1;

        read_literal(&line, &l);
        switch (l.data_type)
        {
            case DATA_F64:
                if (sscanf(line, " %u", &ulps) < 1)
                    ulps = 0;
                if (l.element_count == 1)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_double(rb, &rect, l.v.f64.x, ulps);
                }
                else if (l.element_count == 2)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_dvec2(rb, &rect, &l.v.f64, ulps);
                }
                else
                {
                    fatal_error("Unhandled f64 element count %u.\n", l.element_count);
                }
                break;

            case DATA_F32:
                if (sscanf(line, " %u", &ulps) < 1)
                    ulps = 0;
                if (l.element_count == 1)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_float(rb, &rect, l.v.f32.x, ulps);
                }
                else if (l.element_count == 2)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_vec2(rb, &rect, &l.v.f32, ulps);
                }
                else if (l.element_count == 4)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_vec4(rb, &rect, &l.v.f32, ulps);
                }
                else
                {
                    fatal_error("Unhandled f32 element count %u.\n", l.element_count);
                }
                break;

            case DATA_I32:
            case DATA_U32:
                if (l.element_count == 1)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_uint(rb, &box, l.v.u32.x, 0);
                }
                else if (l.element_count == 4)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_uvec4(rb, &rect, &l.v.u32);
                }
                else
                {
                    fatal_error("Unhandled i32/u32 element count %u.\n", l.element_count);
                }
                break;

            case DATA_I64:
            case DATA_U64:
                if (l.element_count == 1)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_uint64(rb, &box, l.v.u64.x, 0);
                }
                else if (l.element_count == 2)
                {
                    todo_if(runner->is_todo) bug_if(runner->is_bug)
                    check_readback_data_u64vec2(rb, &rect, &l.v.u64);
                }
                else
                {
                    fatal_error("Unhandled i64/u64 element count %u.\n", l.element_count);
                }
                break;

            default:
                fatal_error("Unhandled literal data type %#x.\n", l.data_type);
        }

        runner->ops->release_readback(runner, rb);
    }
    else if (match_string(line, "write", &line))
    {
        unsigned int slot, offset;
        struct uniform_buffer *u;
        enum resource_type type;
        struct literal l;
        int len;

        if (!parse_resource_reference(&line, &type, &slot))
            fatal_error("Malformed resource reference '%s'.\n", line);
        if (type != RESOURCE_TYPE_UNIFORM_BUFFER)
            fatal_error("Unhandled resource type %#x.\n", type);
        if (slot >= ARRAY_SIZE(runner->uniform_buffers))
            fatal_error("Undefined resource.\n");
        u = &runner->uniform_buffers[slot];

        if (sscanf(line, " ( %u )%n", &offset, &len) != 1)
            fatal_error("Malformed write arguments '%s'.\n", line);
        line += len;

        read_literal(&line, &l);
        uniform_buffer_write(u, offset, l.u32_count, &l.v.u32);
    }
    else if (match_string(line, "uniform", &line))
    {
        struct uniform_buffer *u = &runner->uniform_buffers[VKD3D_SHADER_D3DBC_FLOAT_CONSTANT_REGISTER];
        unsigned int offset;

        if (!sscanf(line, "%u", &offset))
            fatal_error("Malformed uniform offset '%s'.\n", line);
        line = strchr(line, ' ') + 1;

        if (match_string(line, "float4", &line))
        {
            struct vec4 v;

            read_f32(&line, &v.x);
            read_f32(&line, &v.y);
            read_f32(&line, &v.z);
            read_f32(&line, &v.w);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else if (match_string(line, "float", &line))
        {
            float f;

            read_f32(&line, &f);
            uniform_buffer_write(u, offset, 1, &f);
        }
        else if (match_string(line, "double2", &line))
        {
            struct dvec2 v;

            read_f64(&line, &v.x);
            read_f64(&line, &v.y);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else if (match_string(line, "int4", &line))
        {
            struct ivec4 v;

            read_int4(&line, &v);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else if (match_string(line, "uint4", &line))
        {
            struct uvec4 v;

            read_uint4(&line, &v);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else if (match_string(line, "int", &line))
        {
            int i;

            read_int(&line, &i, true);
            uniform_buffer_write(u, offset, 1, &i);
        }
        else if (match_string(line, "uint", &line))
        {
            unsigned int v;

            read_uint(&line, &v, true);
            uniform_buffer_write(u, offset, 1, &v);
        }
        else if (match_string(line, "int64_t2", &line))
        {
            struct i64vec2 v;

            read_int64_t2(&line, &v);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else if (match_string(line, "uint64_t2", &line))
        {
            struct u64vec2 v;

            read_uint64_t2(&line, &v);
            uniform_buffer_write(u, offset, 4, &v);
        }
        else
        {
            fatal_error("Unknown uniform type '%s'.\n", line);
        }
    }
    else if (match_string(line, "sample mask", &line))
    {
        unsigned int sample_mask;

        read_uint(&line, &sample_mask, false);
        runner->sample_mask = sample_mask;
    }
    else if (match_string(line, "alpha test", &line))
    {
        runner->alpha_test_func = (enum vkd3d_shader_comparison_func)parse_comparison_func(line, &line);
        runner->alpha_test_ref = strtof(line, &rest);
        line = rest;
    }
    else if (match_string(line, "shade mode", &line))
    {
        if (match_string(line, "flat", &line))
            runner->flat_shading = true;
        else
            runner->flat_shading = false;
    }
    else if (match_string(line, "clip-plane", &line))
    {
        unsigned int index;
        struct vec4 *v;

        index = strtoul(line, (char **)&rest, 10);
        if (rest == line || index >= 8)
            fatal_error("Malformed clip plane directive '%s'.\n", line);
        line = rest;

        v = &runner->clip_planes[index];

        if (match_string(line, "disable", &line))
            runner->clip_plane_mask &= ~(1u << index);
        else
        {
            if (sscanf(line, "%f %f %f %f", &v->x, &v->y, &v->z, &v->w) < 4)
                fatal_error("Malformed float4 constant '%s'.\n", line);
            runner->clip_plane_mask |= (1u << index);
        }
    }
    else if (match_string(line, "point-size", &line))
    {
        runner->point_size = strtof(line, &rest);
        line = rest;
        runner->point_size_min = strtof(line, &rest);
        line = rest;
        runner->point_size_max = strtof(line, &rest);
    }
    else if (match_string(line, "point-sprite", &line))
    {
        if (match_string(line, "on", &line))
            runner->point_sprite = true;
        else
            runner->point_sprite = false;
    }
    else if (match_string(line, "fog", &line))
    {
        if (match_string(line, "disable", &line))
            runner->fog_mode = FOG_MODE_DISABLE;
        else if (match_string(line, "none", &line))
            runner->fog_mode = FOG_MODE_NONE;
        else if (match_string(line, "linear", &line))
            runner->fog_mode = FOG_MODE_LINEAR;
        else if (match_string(line, "exp", &line))
            runner->fog_mode = FOG_MODE_EXP;
        else if (match_string(line, "exp2", &line))
            runner->fog_mode = FOG_MODE_EXP2;
        else
            fatal_error("Invalid fog mode '%s'.\n", line);

        if (match_string(line, "ortho", &line))
            runner->ortho_fog = true;
        if (match_string(line, "non-ortho", &line))
            runner->ortho_fog = false;

        if (runner->fog_mode == FOG_MODE_LINEAR)
        {
            if (sscanf(line, "%f %f", &runner->fog_start, &runner->fog_end) < 2)
                fatal_error("Malformed fog constants '%s'.\n", line);
        }
        else if (runner->fog_mode == FOG_MODE_EXP || runner->fog_mode == FOG_MODE_EXP2)
        {
            if (sscanf(line, "%f", &runner->fog_density) < 1)
                fatal_error("Malformed fog constants '%s'.\n", line);
        }
    }
    else if (match_string(line, "fog-colour", &line))
    {
        struct vec4 *v = &runner->fog_colour;

        if (sscanf(line, "%f %f %f %f", &v->x, &v->y, &v->z, &v->w) < 4)
            fatal_error("Malformed float4 constant '%s'.\n", line);
    }
    else if (match_string(line, "viewport", &line))
    {
        unsigned int i;

        read_uint(&line, &i, false);
        if (i >= ARRAY_SIZE(runner->viewports))
            fatal_error("Unhandled viewport index %u.\n", i);

        if (sscanf(line, " ( %f , %f , %f , %f )", &runner->viewports[i].x, &runner->viewports[i].y,
                &runner->viewports[i].width, &runner->viewports[i].height) < 4)
            fatal_error("Malformed viewport '%s'.\n", line);

        runner->viewport_count = max(runner->viewport_count, i + 1);
    }
    else if (match_string(line, "bump", &line))
    {
        struct bump_constants *c;
        unsigned int index;

        index = strtoul(line, (char **)&rest, 10);
        if (rest == line || index >= 6)
            fatal_error("Malformed bump directive '%s'.\n", line);
        line = rest;

        c = &runner->bump[index];

        if (!match_string(line, "f32", &line))
            fatal_error("Malformed bump directive '%s'.\n", line);
        if (*line != '(')
            fatal_error("Malformed bump directive '%s'.\n", line);
        ++line;
        read_f32(&line, &c->matrix[0][0]);
        if (!match_string(line, ",", &line))
            fatal_error("Malformed bump directive '%s'.\n", line);
        read_f32(&line, &c->matrix[0][1]);
        if (!match_string(line, ",", &line))
            fatal_error("Malformed bump directive '%s'.\n", line);
        read_f32(&line, &c->matrix[1][0]);
        if (!match_string(line, ",", &line))
            fatal_error("Malformed bump directive '%s'.\n", line);
        read_f32(&line, &c->matrix[1][1]);
        if (*line != ')')
            fatal_error("Malformed bump directive '%s'.\n", line);
        ++line;

        read_f32(&line, &c->luminance_scale);
        read_f32(&line, &c->luminance_offset);
    }
    else if (match_string(line, "denorm", &line))
    {
        if (match_string(line, "preserve", &line))
            runner->denorm_mode = DENORM_PRESERVE;
        else if (match_string(line, "ftz", &line))
            runner->denorm_mode = DENORM_FTZ;
        else if (match_string(line, "any", &line))
            runner->denorm_mode = DENORM_ANY;
        else
            fatal_error("Invalid denorm mode '%s'.\n", line);
    }
    else if (match_string(line, "projected", &line))
    {
        unsigned int index;

        index = strtoul(line, (char **)&rest, 10);
        if (rest == line || index >= 6)
            fatal_error("Malformed projection directive '%s'.\n", line);
        line = rest;

        if (match_string(line, "enable", &line))
            runner->projected_texture_mask |= (1u << index);
        else
            runner->projected_texture_mask &= ~(1u << index);
    }
    else
    {
        fatal_error("Unknown test directive '%s'.\n", line);
    }
}

static void parse_descriptors_directive(struct shader_runner *runner, const char *line)
{
    struct descriptor_mapping *descriptor;
    const char *target_template;
    unsigned int register_to;
    char type_chr;
    int pos;

    runner->default_descriptors = false;

    if (runner->descriptor_count == ARRAY_SIZE(runner->descriptors))
        fatal_error("Too many descriptors (%u).\n", runner->descriptor_count);

    descriptor = &runner->descriptors[runner->descriptor_count++];

    if (sscanf(line, " %c [ %u : %u ] , space %u ,%n", &type_chr, &descriptor->register_idx,
            &register_to, &descriptor->register_space, &pos) < 4)
        fatal_error("Malformed descriptors directive '%s'.\n", line);

    descriptor->count = register_to - descriptor->register_idx + 1;

    switch (type_chr)
    {
        case 't':
            descriptor->type = VKD3D_SHADER_DESCRIPTOR_TYPE_SRV;
            target_template = " srv %u";
            break;

        case 'u':
            descriptor->type = VKD3D_SHADER_DESCRIPTOR_TYPE_UAV;
            target_template = " uav %u";
            break;

        /* Samplers and CBVs are not supported yet. */

        default:
            fatal_error("Malformed descriptors directive '%s'.\n", line);
            break;
    }

    if (sscanf(&line[pos], target_template, &descriptor->target_idx) < 1)
        fatal_error("Malformed descriptors directive '%s'.\n", line);
}

struct sampler *shader_runner_get_sampler(struct shader_runner *runner, unsigned int slot)
{
    struct sampler *sampler;
    size_t i;

    for (i = 0; i < runner->sampler_count; ++i)
    {
        sampler = &runner->samplers[i];

        if (sampler->slot == slot)
            return sampler;
    }

    return NULL;
}

unsigned int get_vb_stride(const struct shader_runner *runner, unsigned int slot)
{
    unsigned int stride = 0;
    size_t i;

    /* We currently don't deal with vertex formats less than 32 bits, so don't
     * bother with alignment. */
    for (i = 0; i < runner->input.element_count; ++i)
    {
        const struct input_element *element = &runner->input.elements[i];

        if (element->slot == slot)
            stride += element->texel_size;
    }

    return stride;
}

static HRESULT map_special_hrs(HRESULT hr)
{
    if (hr == 0x88760b59)
    {
        trace("Mapping hr %#x (D3DXERR_INVALIDDATA) as %#x.\n", hr, E_FAIL);
        return E_FAIL;
    }
    if (hr == 0x80010064)
    {
        trace("Mapping unidentified hr %#x as %#x.\n", hr, E_FAIL);
        return E_FAIL;
    }
    return hr;
}

const char *shader_type_string(enum shader_type type)
{
    static const char *const shader_types[] =
    {
        [SHADER_TYPE_CS] = "cs",
        [SHADER_TYPE_PS] = "ps",
        [SHADER_TYPE_VS] = "vs",
        [SHADER_TYPE_HS] = "hs",
        [SHADER_TYPE_DS] = "ds",
        [SHADER_TYPE_GS] = "gs",
        [SHADER_TYPE_FX] = "fx",
    };
    assert(type < ARRAY_SIZE(shader_types));
    return shader_types[type];
}

static HRESULT d3d10_blob_from_vkd3d_shader_code(const struct vkd3d_shader_code *blob, ID3D10Blob **blob_out)
{
    ID3D10Blob *d3d_blob;
    HRESULT hr;

    if (FAILED(hr = D3DCreateBlob(blob->size, (ID3DBlob **)&d3d_blob)))
    {
        trace("Failed to create blob, hr %#x.\n", hr);
        return hr;
    }

    memcpy(ID3D10Blob_GetBufferPointer(d3d_blob), blob->code, blob->size);
    *blob_out = d3d_blob;

    return S_OK;
}

static HRESULT dxc_compiler_compile_shader(void *dxc_compiler, const char *profile, unsigned int compile_options,
        bool enable_16bit_types, enum denorm_mode denorm_mode, const char *hlsl, ID3D10Blob **blob_out)
{
    struct vkd3d_shader_code blob;
    WCHAR wprofile[7];
    HRESULT hr;

    *blob_out = NULL;

    swprintf(wprofile, ARRAY_SIZE(wprofile), L"%hs", profile);
    if (FAILED(hr = dxc_compile(dxc_compiler, wprofile, compile_options,
            enable_16bit_types, denorm_mode, hlsl, &blob)))
        return hr;

    hr = d3d10_blob_from_vkd3d_shader_code(&blob, blob_out);
    free((void *)blob.code);

    return hr;
}

static ID3D10Blob *parse_hex(const char *source)
{
    size_t len = strlen(source), i, pos = 0, nibble = 0;
    uint32_t *ptr, value = 0;
    ID3D10Blob *blob;

    ptr = malloc(len / 2);

    for (i = 0; i < len; ++i)
    {
        char c = source[i];

        if (nibble == 8)
        {
            ptr[pos++] = value;
            nibble = 0;
            value = 0;
        }

        if (isspace(c))
        {
            if (nibble && nibble != 8)
                fatal_error("Malformed hex literal.\n");
            continue;
        }

        if (c == '%')
        {
            if (nibble && nibble != 8)
                fatal_error("Malformed hex literal.\n");
            while (source[i] != '\n')
                ++i;
            continue;
        }

        value <<= 4;
        if ('0' <= c && c <= '9')
            value |= (c - '0');
        else if ('a' <= c && c <= 'f')
            value |= (c - 'a' + 10);
        else if ('A' <= c && c <= 'F')
            value |= (c - 'A' + 10);
        else
            fatal_error("Invalid hex character '%c'\n", c);
        ++nibble;
    }

    if (nibble == 8)
        ptr[pos++] = value;
    else if (nibble)
        fatal_error("Malformed hex literal.\n");

    D3DCreateBlob(pos * sizeof(value), &blob);
    if (pos)
        memcpy(ID3D10Blob_GetBufferPointer(blob), ptr, pos * sizeof(value));
    free(ptr);

    return blob;
}

ID3D10Blob *compile_hlsl(const struct shader_runner *runner, enum shader_type type)
{
    const char *source = runner->shader_source[type];
    unsigned int options = runner->compile_options;
    ID3D10Blob *blob = NULL, *errors = NULL;
    HRESULT hr = S_OK;
    char profile[7];

    /* Behaviour is inconsistent between different versions of
     * d3dcompiler_47.dll. Version 10.0.17134.12 seems to reject
     * D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY for 5.1 profiles, while
     * version 10.0.10150.0 apparently doesn't. */
    if (runner->shader_model >= SHADER_MODEL_5_1)
        options &= ~D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

    sprintf(profile, "%s_%s", shader_type_string(type), sm_strings_underscore[runner->shader_model]);

    switch (runner->shader_format[type])
    {
        case SOURCE_FORMAT_HLSL:
            if (runner->shader_model >= SHADER_MODEL_6_0)
            {
                assert(runner->dxc_compiler);
                hr = dxc_compiler_compile_shader(runner->dxc_compiler, profile, options,
                        runner->require_shader_caps[SHADER_CAP_NATIVE_16_BIT], runner->denorm_mode, source, &blob);
            }
            else
            {
                hr = D3DCompile(source, strlen(source), NULL, NULL, NULL, "main", profile, options, 0, &blob, &errors);
            }
            break;

        case SOURCE_FORMAT_D3DBC_HEX:
            if (runner->shader_model >= SHADER_MODEL_4_0)
                fatal_error("Cannot use d3dbc-hex with shader model %#x.\n", runner->shader_model);
            blob = parse_hex(source);
            hr = S_OK;
            break;

        case SOURCE_FORMAT_DXBC_TPF_HEX:
            if (runner->shader_model < SHADER_MODEL_4_0 || runner->shader_model >= SHADER_MODEL_6_0)
                fatal_error("Cannot use dxbc-tpf-hex with shader model %#x.\n", runner->shader_model);
            blob = parse_hex(source);
            hr = S_OK;
            break;

        case SOURCE_FORMAT_DXBC_DXIL_HEX:
            if (runner->shader_model < SHADER_MODEL_6_0)
                fatal_error("Cannot use dxbc-dxil-hex with shader model %#x.\n", runner->shader_model);
            blob = parse_hex(source);
            hr = S_OK;
            break;
    }

    if (hr != S_OK)
    {
        todo_if (runner->is_todo)
            ok(false, "Failed to compile shader, hr %#x.\n", hr);
    }
    if (errors)
    {
        if (vkd3d_test_state.debug_level)
            trace("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
    return blob;
}

static void compile_shader(struct shader_runner *runner, const char *source,
        size_t len, enum shader_type type, enum shader_model model)
{
    bool use_dxcompiler = (model >= SHADER_MODEL_6_0);
    unsigned int options = runner->compile_options;
    ID3D10Blob *blob = NULL, *errors = NULL;
    char profile[7];
    HRESULT hr;

    switch (runner->shader_format[type])
    {
        case SOURCE_FORMAT_HLSL:
            break;

        case SOURCE_FORMAT_D3DBC_HEX:
        case SOURCE_FORMAT_DXBC_TPF_HEX:
        case SOURCE_FORMAT_DXBC_DXIL_HEX:
            return;
    }

    /* We can let this go through D3DCompile() with the invalid shader model
     * string, but it returns a unique error code. Just skip it. */
    if (model < SHADER_MODEL_4_0 && type != SHADER_TYPE_VS && type != SHADER_TYPE_PS && type != SHADER_TYPE_FX)
        return;
    if (model < SHADER_MODEL_5_0 && (type == SHADER_TYPE_HS || type == SHADER_TYPE_DS))
        return;

    /* Behaviour is inconsistent between different versions of
     * d3dcompiler_47.dll. Version 10.0.17134.12 seems to reject
     * D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY for 5.1 profiles, while
     * version 10.0.10150.0 apparently doesn't. */
    if (model >= SHADER_MODEL_5_1)
        options &= ~D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

    sprintf(profile, "%s_%s", shader_type_string(type), sm_strings_underscore[model]);

    if (use_dxcompiler)
    {
        assert(runner->dxc_compiler);
        hr = dxc_compiler_compile_shader(runner->dxc_compiler, profile, options,
                runner->require_shader_caps[SHADER_CAP_NATIVE_16_BIT], runner->denorm_mode, source, &blob);
    }
    else
    {
        hr = D3DCompile(source, len, NULL, NULL, NULL, "main", profile, options, 0, &blob, &errors);
    }
    hr = map_special_hrs(hr);
    todo_if (runner->hlsl_todo[model])
        ok(hr == runner->hlsl_hrs[model], "Got unexpected hr %#x.\n", hr);
    if (hr == S_OK)
    {
        ID3D10Blob_Release(blob);
    }
    else
    {
        assert_that(!blob, "Expected no compiled shader blob.\n");
        if (!use_dxcompiler)
            assert_that(!!errors, "Expected non-NULL error blob.\n");
    }
    if (errors)
    {
        if (vkd3d_test_state.debug_level)
            trace("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
        ID3D10Blob_Release(errors);
    }
}

static void read_shader_directive(struct shader_runner *runner, const char *line,
        const char *src, enum shader_type shader_type)
{
    for (unsigned int i = SHADER_MODEL_MIN; i <= SHADER_MODEL_MAX; ++i)
    {
        runner->hlsl_hrs[i] = S_OK;
        runner->hlsl_todo[i] = false;
    }

    runner->shader_format[shader_type] = SOURCE_FORMAT_HLSL;

    while (*src && *src != ']')
    {
        uint32_t model_mask;

        if (match_string(src, "todo", &src))
        {
            check_qualifier_args(runner, src, &src, &model_mask);
            for (unsigned int i = SHADER_MODEL_MIN; i <= SHADER_MODEL_MAX; ++i)
            {
                /* 'todo' is not meaningful when dxcompiler is in use. */
                if (i < SHADER_MODEL_6_0 && (model_mask & (1u << i)))
                    runner->hlsl_todo[i] = true;
            }
        }
        else if (match_string(src, "fail", &src))
        {
            check_qualifier_args(runner, src, &src, &model_mask);
            for (unsigned int i = SHADER_MODEL_MIN; i <= SHADER_MODEL_MAX; ++i)
            {
                if (model_mask & (1u << i))
                    runner->hlsl_hrs[i] = E_FAIL;
            }
        }
        else if (match_string(src, "notimpl", &src))
        {
            check_qualifier_args(runner, src, &src, &model_mask);
            for (unsigned int i = SHADER_MODEL_MIN; i <= SHADER_MODEL_MAX; ++i)
            {
                if (model_mask & (1u << i))
                    runner->hlsl_hrs[i] = E_NOTIMPL;
            }
        }
        else if (match_string(src, "d3dbc-hex", &src))
        {
            runner->shader_format[shader_type] = SOURCE_FORMAT_D3DBC_HEX;
        }
        else if (match_string(src, "dxbc-tpf-hex", &src))
        {
            runner->shader_format[shader_type] = SOURCE_FORMAT_DXBC_TPF_HEX;
        }
        else if (match_string(src, "dxbc-dxil-hex", &src))
        {
            runner->shader_format[shader_type] = SOURCE_FORMAT_DXBC_DXIL_HEX;
        }
        else
        {
            fatal_error("Malformed line '%s'.\n", line);
        }
    }

    if (strcmp(src, "]\n"))
        fatal_error("Malformed line '%s'.\n", line);
}

static bool check_capabilities(const struct shader_runner *runner, const struct shader_runner_caps *caps)
{
    unsigned int i;

    for (i = 0; i < SHADER_CAP_COUNT; ++i)
    {
        if (runner->require_shader_caps[i] && !caps->shader_caps[i])
            return false;
    }

    for (i = 0; i < ARRAY_SIZE(runner->require_format_caps); ++i)
    {
        if (runner->require_format_caps[i] & ~caps->format_caps[i])
            return false;
    }
    return true;
}

static void trace_tags(const struct shader_runner_caps *caps)
{
    size_t i, rem, count = 0;
    char tags[80], *p;
    int rc;

    for (i = 0; i < ARRAY_SIZE(caps->tags); ++i)
    {
        count += caps->tags[i].present;
    }

    if (!count)
        return;

    p = tags;
    rem = ARRAY_SIZE(tags);
    rc = snprintf(p, rem, "%8s:", "tags");
    p += rc;
    rem -= rc;

    for (i = 0; i < ARRAY_SIZE(caps->tags); ++i)
    {
        char version[64] = "";

        if (!caps->tags[i].present)
            continue;

        if (caps->tags[i].major != 0 || caps->tags[i].minor != 0 || caps->tags[i].patch != 0)
            sprintf(version, " (%u.%u.%u)", caps->tags[i].major, caps->tags[i].minor, caps->tags[i].patch);

        rc = snprintf(p, rem, " \"%s\"%s%s", shader_runner_tag_names[i], version, count == 1 ? "" : ",");
        if (!(rc >= 0 && (size_t)rc < rem))
        {
            *p = 0;
            trace("%s\n", tags);

            p = tags;
            rem = ARRAY_SIZE(tags);
            rc = snprintf(p, rem, "%8s ", "");
            --i;
        }
        else
        {
            --count;
        }
        p += rc;
        rem -= rc;
    }
    trace("%s.\n", tags);
}

static void trace_shader_caps(const bool *caps)
{
    bool show_none = true;
    char buffer[80], *p;
    size_t rem;
    int rc;

    p = buffer;
    rem = ARRAY_SIZE(buffer);
    rc = snprintf(p, rem, "%8s:", "caps");
    p += rc;
    rem -= rc;

    for (size_t i = 0; i < SHADER_CAP_COUNT; ++i)
    {
        if (!caps[i])
            continue;

        rc = snprintf(p, rem, " %s", shader_cap_strings[i]);
        if (!(rc >= 0 && (size_t)rc < rem))
        {
            *p = 0;
            trace("%s\n", buffer);

            p = buffer;
            rem = ARRAY_SIZE(buffer);
            rc = snprintf(p, rem, "%8s ", "");
            --i;
        }
        p += rc;
        rem -= rc;
        show_none = false;
    }
    if (show_none)
        snprintf(p, rem, " (none)");
    trace("%s.\n", buffer);
}

static void trace_format_cap(const struct shader_runner_caps *caps, enum format_cap cap, const char *cap_name)
{
    bool show_none = true;
    char buffer[80], *p;
    const char *name;
    size_t rem;
    int rc;

    p = buffer;
    rem = ARRAY_SIZE(buffer);
    rc = snprintf(p, rem, "%8s:", cap_name);
    p += rc;
    rem -= rc;

    for (unsigned int i = 0; i < ARRAY_SIZE(caps->format_caps); ++i)
    {
        if (caps->format_caps[i] & cap)
        {
            if ((name = get_format_name(i)))
                rc = snprintf(p, rem, " %s", name);
            else
                rc = snprintf(p, rem, " 0x%x", i);
            if (!(rc >= 0 && (size_t)rc < rem))
            {
                *p = 0;
                trace("%s\n", buffer);

                p = buffer;
                rem = ARRAY_SIZE(buffer);
                rc = snprintf(p, rem, "%8s ", "");
                --i;
            }
            p += rc;
            rem -= rc;
            show_none = false;
        }
    }
    if (show_none)
        snprintf(p, rem, " (none)");
    trace("%s.\n", buffer);
}

static void update_line_number_context(const char *testname, unsigned int line_number, enum shader_model model)
{
    vkd3d_test_pop_context();
    vkd3d_test_push_context("%s:%u: Model %s", testname, line_number, sm_strings_dot[model]);
}

enum test_action
{
    TEST_ACTION_COMPILE,
    TEST_ACTION_RUN,
};

bool test_skipping_execution(const char *executor, const char *compiler,
        enum shader_model minimum_shader_model, enum shader_model maximum_shader_model)
{
    if (shader_test_options.executor_filter
            && strcmp(shader_test_options.executor_filter, executor))
    {
        trace("Skipping compiling shaders with %s and executing with %s "
                "because of the executor filter.\n",
                compiler, executor);
        return true;
    }

    if (shader_test_options.compiler_filter
            && strcmp(shader_test_options.compiler_filter, compiler))
    {
        trace("Skipping compiling shaders with %s and executing with %s "
                "because of the compiler filter.\n",
                compiler, executor);
        return true;
    }

    minimum_shader_model = max(minimum_shader_model, shader_test_options.minimum_shader_model);
    maximum_shader_model = min(maximum_shader_model, shader_test_options.maximum_shader_model);

    if (minimum_shader_model > maximum_shader_model)
    {
        trace("Skipping compiling shaders with %s and executing with %s "
                "because the shader model range is empty.\n",
                compiler, executor);
        return true;
    }

    return false;
}

static int compare_resources(const void *ptr1, const void *ptr2)
{
    const struct resource *resource1 = *(const struct resource **)ptr1;
    const struct resource *resource2 = *(const struct resource **)ptr2;
    int ret;

    if ((ret = vkd3d_u32_compare(resource1->desc.type, resource2->desc.type)))
        return ret;

    return vkd3d_u32_compare(resource1->desc.slot, resource2->desc.slot);
}

static void set_default_descriptor_mapping(struct shader_runner *runner)
{
    struct descriptor_mapping *mapping, *base_mapping = NULL;
    bool is_buffer = false;
    size_t i;

    qsort(runner->resources, runner->resource_count, sizeof(*runner->resources), compare_resources);

    runner->descriptor_count = 0;

    for (i = 0; i < runner->resource_count; ++i)
    {
        const struct resource *resource = runner->resources[i];

        switch (resource->desc.type)
        {
            case RESOURCE_TYPE_TEXTURE:
            case RESOURCE_TYPE_UAV:
                mapping = &runner->descriptors[runner->descriptor_count];
                if (resource->desc.type == RESOURCE_TYPE_UAV)
                    mapping->type = VKD3D_SHADER_DESCRIPTOR_TYPE_UAV;
                else
                    mapping->type = VKD3D_SHADER_DESCRIPTOR_TYPE_SRV;
                mapping->register_idx = resource->desc.slot;
                mapping->register_space = 0;
                mapping->count = 1;
                mapping->target_idx = resource->desc.slot;

                if (base_mapping && base_mapping->type == mapping->type
                        && is_buffer == (resource->desc.dimension == RESOURCE_DIMENSION_BUFFER)
                        && base_mapping->register_idx + base_mapping->count == resource->desc.slot)
                {
                    ++base_mapping->count;
                    break;
                }

                base_mapping = mapping;
                is_buffer = resource->desc.dimension == RESOURCE_DIMENSION_BUFFER;
                ++runner->descriptor_count;
                break;

            case RESOURCE_TYPE_RENDER_TARGET:
            case RESOURCE_TYPE_DEPTH_STENCIL:
            case RESOURCE_TYPE_VERTEX_BUFFER:
            case RESOURCE_TYPE_UNIFORM_BUFFER:
                break;
        }
    }
}

static void add_one_version_from(uint32_t *model_mask, struct shader_runner *runner, uint32_t from)
{
    for (unsigned int j = runner->minimum_shader_model; j <= runner->maximum_shader_model; ++j)
    {
        if (bitmap_is_set(&from, j) && runner->hlsl_hrs[j] == S_OK)
        {
            bitmap_set(model_mask, j);
            break;
        }
    }
}

static uint32_t get_runner_test_mask(struct shader_runner *runner, enum test_action test_action)
{
    uint32_t model_mask = runner->test_model_mask;

    static const uint32_t sm1_mask = (1u << SHADER_MODEL_4_0) - 1;
    static const uint32_t sm4_mask = ((1u << SHADER_MODEL_6_0) - 1) & ~sm1_mask;
    static const uint32_t sm6_mask = ((1u << (SHADER_MODEL_MAX + 1)) - 1) & ~(sm1_mask | sm4_mask);

    if (test_action == TEST_ACTION_COMPILE)
    {
        if (runner->hlsl_hrs[runner->minimum_shader_model] == S_OK)
            bitmap_set(&model_mask, runner->minimum_shader_model);
        for (unsigned int j = SHADER_MODEL_MIN + 1; j <= SHADER_MODEL_MAX; ++j)
        {
            if (runner->hlsl_hrs[j - 1] != runner->hlsl_hrs[j])
            {
                /* Behaviour changes here; test both versions. */
                bitmap_set(&model_mask, j - 1);
                bitmap_set(&model_mask, j);
            }
        }
    }

    /* Most versions have relatively minor differences between them, so don't
     * waste time testing every version possible. SM1-3, SM4-5, and SM6 are
     * completely different formats, often with different HLSL semantics, so
     * where possible try to test one version from each set. */
    if (!(model_mask & sm1_mask))
        add_one_version_from(&model_mask, runner, sm1_mask);
    if (!(model_mask & sm4_mask))
        add_one_version_from(&model_mask, runner, sm4_mask);
    if (!(model_mask & sm6_mask))
        add_one_version_from(&model_mask, runner, sm6_mask);

    if (test_action == TEST_ACTION_COMPILE)
    {
        /* If the test is expected to fail for all versions in a group,
         * confirm that by adding one version. */
        if (!(model_mask & sm1_mask))
            bitmap_set(&model_mask, SHADER_MODEL_2_0);
        if (!(model_mask & sm4_mask))
            bitmap_set(&model_mask, SHADER_MODEL_4_0);
        if (!(model_mask & sm6_mask))
            bitmap_set(&model_mask, SHADER_MODEL_6_0);
    }

    return model_mask;
}

static bool should_skip_execution(struct shader_runner *runner, enum test_action action)
{
    uint32_t model_mask;

    if (runner->skip_directives)
        return true;
    if (action != (runner->ops ? TEST_ACTION_RUN : TEST_ACTION_COMPILE))
        return true;
    if (runner->shader_model > runner->maximum_shader_model || runner->shader_model < runner->minimum_shader_model)
        return true;

    /* Compilation which fails with dxcompiler is not 'todo', therefore the tests are
     * not 'todo' either. They cannot run, so skip them entirely. */
    if (action == TEST_ACTION_RUN && FAILED(runner->hlsl_hrs[runner->shader_model]))
        return true;

    model_mask = get_runner_test_mask(runner, action);
    return !bitmap_is_set(&model_mask, runner->shader_model);
}

static void run_shader_tests_for_model(struct shader_runner *runner, FILE *f)
{
    const enum test_action test_action = runner->ops ? TEST_ACTION_RUN : TEST_ACTION_COMPILE;
    unsigned int i, line_number = 0, block_start_line_number = 0;
    size_t shader_source_size = 0, shader_source_len = 0;
    bool skip_test_section = false, missing_caps = false;
    enum shader_type shader_type = SHADER_TYPE_CS;
    struct resource_params current_resource;
    struct sampler *current_sampler = NULL;
    enum parse_state state = STATE_NONE;
    char *shader_source = NULL;
    char line_buffer[256];
    const char *testname;

    if ((testname = strrchr(shader_test_options.filename, '/')))
        ++testname;
    else
        testname = shader_test_options.filename;

    vkd3d_test_push_context("%s:%u: Model %s", testname, line_number, sm_strings_dot[runner->shader_model]);

    runner->default_descriptors = true;
    runner->alpha_test_func = VKD3D_SHADER_COMPARISON_FUNC_ALWAYS;
    runner->point_size = 1.0f;
    runner->point_size_min = 1.0f;
    runner->point_size_max = FLT_MAX;
    runner->fog_mode = FOG_MODE_DISABLE;
    runner->fog_start = 0.0f;
    runner->fog_end = 1.0f;
    runner->fog_density = 1.0f;

    runner->sample_mask = ~0u;
    runner->depth_bounds = false;
    runner->depth_min = 0.0f;
    runner->depth_max = 1.0f;

    for (;;)
    {
        char *ret = fgets(line_buffer, sizeof(line_buffer), f);
        const char *line = line_buffer;

        line_number++;

        if (!ret || line[0] == '[')
        {
            update_line_number_context(testname, block_start_line_number, runner->shader_model);

            switch (state)
            {
                case STATE_INPUT_LAYOUT:
                case STATE_NONE:
                case STATE_SAMPLER:
                case STATE_DESCRIPTORS:
                case STATE_HEADER:
                    break;

                case STATE_TEST:
                    if (missing_caps)
                        skip("Missing capabilities/tags.\n");
                    break;

                case STATE_REQUIRE:
                    if (runner->maximum_shader_model < runner->minimum_shader_model)
                    {
                        runner->skip_directives = true;
                    }
                    else if (!check_capabilities(runner, runner->caps))
                    {
                        runner->skip_directives = true;
                        missing_caps = true;
                    }
                    break;

                case STATE_RESOURCE:
                    if (current_resource.desc.type == RESOURCE_TYPE_VERTEX_BUFFER)
                        current_resource.desc.width = current_resource.data_size;

                    if (current_resource.desc.type == RESOURCE_TYPE_UAV)
                        assert_that(current_resource.explicit_format, "Format must be specified for UAV resources.\n");

                    /* Not every backend supports every resource type
                     * (specifically, D3D9 doesn't support UAVs and
                     * textures with data type other than float). */
                    if (test_action == TEST_ACTION_RUN && !runner->skip_directives)
                        set_resource(runner, &current_resource);
                    free(current_resource.data);
                    break;

                case STATE_SHADER:
                    if (!should_skip_execution(runner, TEST_ACTION_COMPILE))
                        compile_shader(runner, shader_source, shader_source_len, shader_type, runner->shader_model);

                    free(runner->shader_source[shader_type]);
                    runner->shader_source[shader_type] = shader_source;
                    shader_source = NULL;
                    shader_source_len = 0;
                    shader_source_size = 0;
                    break;

                case STATE_PREPROC_INVALID:
                {
                    ID3D10Blob *blob = NULL, *errors = NULL;
                    HRESULT hr;

                    if (test_action != TEST_ACTION_COMPILE || runner->skip_directives)
                        break;

                    hr = D3DPreprocess(shader_source, strlen(shader_source), NULL, NULL, NULL, &blob, &errors);
                    ok(hr == E_FAIL, "Got unexpected hr %#x.\n", hr);
                    ok(!blob, "Expected no compiled shader blob.\n");
                    ok(!!errors, "Expected non-NULL error blob.\n");

                    if (errors)
                    {
                        if (vkd3d_test_state.debug_level)
                            trace("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
                        ID3D10Blob_Release(errors);
                    }

                    shader_source_len = 0;
                    break;
                }

                case STATE_PREPROC:
                {
                    ID3D10Blob *blob = NULL, *errors = NULL;
                    SIZE_T size;
                    HRESULT hr;
                    char *text;

                    if (test_action != TEST_ACTION_COMPILE || runner->skip_directives)
                        break;

                    hr = D3DPreprocess(shader_source, strlen(shader_source), NULL, NULL, NULL, &blob, &errors);
                    ok(hr == S_OK, "Got unexpected hr %#x.\n", hr);
                    if (hr == S_OK)
                    {
                        if (errors)
                        {
                            if (vkd3d_test_state.debug_level)
                                trace("%s\n", (char *)ID3D10Blob_GetBufferPointer(errors));
                            ID3D10Blob_Release(errors);
                        }

                        text = ID3D10Blob_GetBufferPointer(blob);
                        size = ID3D10Blob_GetBufferSize(blob);
                        ok(vkd3d_memmem(text, size, "pass", strlen("pass")),
                                "'pass' not found in preprocessed shader.\n");
                        ok(!vkd3d_memmem(text, size, "fail", strlen("fail")),
                                "'fail' found in preprocessed shader.\n");
                        ID3D10Blob_Release(blob);
                    }

                    shader_source_len = 0;
                    break;
                }
            }

            if (!ret)
                break;
        }

        if (line[0] == '[')
        {
            unsigned int index;

            block_start_line_number = line_number;
            update_line_number_context(testname, line_number, runner->shader_model);

            if (match_string(line, "[compute shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_CS;
            }
            else if (!strcmp(line, "[require]\n"))
            {
                state = STATE_REQUIRE;
                runner->minimum_shader_model = runner->caps->minimum_shader_model;
                runner->maximum_shader_model = runner->caps->maximum_shader_model;
                memset(runner->require_shader_caps, 0, sizeof(runner->require_shader_caps));
                memset(runner->require_format_caps, 0, sizeof(runner->require_format_caps));
                runner->compile_options = 0;
                runner->skip_directives = false;
                missing_caps = false;
            }
            else if (match_string(line, "[pixel shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_PS;
            }
            else if (sscanf(line, "[sampler %u]\n", &index))
            {
                state = STATE_SAMPLER;

                if (!(current_sampler = shader_runner_get_sampler(runner, index)))
                {
                    if (runner->sampler_count == MAX_SAMPLERS)
                        fatal_error("Too many samplers declared.\n");

                    current_sampler = &runner->samplers[runner->sampler_count++];
                }
                memset(current_sampler, 0, sizeof(*current_sampler));
                current_sampler->slot = index;
                current_sampler->filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                current_sampler->u_address = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                current_sampler->v_address = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                current_sampler->w_address = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            }
            else if (sscanf(line, "[rtv %u]\n", &index))
            {
                state = STATE_RESOURCE;

                memset(&current_resource, 0, sizeof(current_resource));

                current_resource.desc.slot = index;
                current_resource.desc.type = RESOURCE_TYPE_RENDER_TARGET;
                current_resource.desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                current_resource.desc.texel_size = 16;
                current_resource.desc.level_count = 1;
            }
            else if (!strcmp(line, "[dsv]\n"))
            {
                state = STATE_RESOURCE;

                memset(&current_resource, 0, sizeof(current_resource));

                current_resource.desc.slot = 0;
                current_resource.desc.type = RESOURCE_TYPE_DEPTH_STENCIL;
                current_resource.desc.format = DXGI_FORMAT_D32_FLOAT;
                current_resource.is_shadow = true;
                current_resource.desc.texel_size = 4;
                current_resource.desc.level_count = 1;
            }
            else if (sscanf(line, "[srv %u]\n", &index))
            {
                state = STATE_RESOURCE;

                memset(&current_resource, 0, sizeof(current_resource));

                current_resource.desc.slot = index;
                current_resource.desc.type = RESOURCE_TYPE_TEXTURE;
                current_resource.desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                current_resource.desc.texel_size = 16;
                current_resource.desc.level_count = 1;
            }
            else if (sscanf(line, "[uav %u]\n", &index))
            {
                state = STATE_RESOURCE;

                memset(&current_resource, 0, sizeof(current_resource));

                current_resource.desc.slot = index;
                current_resource.desc.type = RESOURCE_TYPE_UAV;
                current_resource.desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                current_resource.desc.texel_size = 16;
                current_resource.desc.level_count = 1;
            }
            else if (sscanf(line, "[vb %u]\n", &index))
            {
                state = STATE_RESOURCE;

                memset(&current_resource, 0, sizeof(current_resource));

                current_resource.desc.slot = index;
                current_resource.desc.type = RESOURCE_TYPE_VERTEX_BUFFER;
                current_resource.desc.dimension = RESOURCE_DIMENSION_BUFFER;
            }
            else if (!strcmp(line, "[test]\n"))
            {
                state = STATE_TEST;
                skip_test_section = should_skip_execution(runner, TEST_ACTION_RUN);
                if (runner->default_descriptors)
                    set_default_descriptor_mapping(runner);
            }
            else if (!strcmp(line, "[preproc]\n"))
            {
                state = STATE_PREPROC;
            }
            else if (!strcmp(line, "[preproc fail]\n"))
            {
                state = STATE_PREPROC_INVALID;
            }
            else if (match_string(line, "[vertex shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_VS;
            }
            else if (match_string(line, "[effect", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_FX;
            }
            else if (match_string(line, "[hull shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_HS;
            }
            else if (match_string(line, "[domain shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_DS;
            }
            else if (match_string(line, "[geometry shader", &line))
            {
                state = STATE_SHADER;
                shader_type = SHADER_TYPE_GS;
            }
            else if (!strcmp(line, "[input layout]\n"))
            {
                state = STATE_INPUT_LAYOUT;

                input_layout_clear(&runner->input);
            }
            else if (!strcmp(line, "[descriptors]\n"))
            {
                state = STATE_DESCRIPTORS;
                runner->descriptor_count = 0;
                runner->default_descriptors = true;
            }
            else if (!strcmp(line, "[header]\n"))
            {
                if (state != STATE_NONE && state != STATE_HEADER)
                    fatal_error("The [header] section must be at the top.");
                state = STATE_HEADER;
            }
            else
            {
                fatal_error("Unknown directive '%s'.\n", line);
            }

            if (state == STATE_SHADER)
                read_shader_directive(runner, line_buffer, line, shader_type);
        }
        else if (line[0] != '%' && line[0] != '\n')
        {
            update_line_number_context(testname, line_number, runner->shader_model);

            switch (state)
            {
                case STATE_NONE:
                    fatal_error("Malformed line '%s'.\n", line);
                    break;

                case STATE_HEADER:
                    /* The header section is already parsed in parse_header(). */
                    break;

                case STATE_INPUT_LAYOUT:
                    parse_input_layout_directive(runner, line);
                    break;

                case STATE_PREPROC:
                case STATE_PREPROC_INVALID:
                case STATE_SHADER:
                {
                    size_t len = strlen(line);

                    vkd3d_array_reserve((void **)&shader_source, &shader_source_size, shader_source_len + len + 1, 1);
                    memcpy(shader_source + shader_source_len, line, len + 1);
                    shader_source_len += len;
                    break;
                }

                case STATE_REQUIRE:
                    parse_require_directive(runner, line);
                    break;

                case STATE_RESOURCE:
                    parse_resource_directive(&current_resource, line);
                    break;

                case STATE_SAMPLER:
                    parse_sampler_directive(current_sampler, line);
                    break;

                case STATE_TEST:
                    if (!skip_test_section && !runner->failed_resource_count)
                        parse_test_directive(runner, line);
                    break;

                case STATE_DESCRIPTORS:
                    parse_descriptors_directive(runner, line);
                    break;
            }
        }
    }

    /* Pop line_number context. */
    vkd3d_test_pop_context();

    input_layout_cleanup(&runner->input);
    for (i = 0; i < SHADER_TYPE_COUNT; ++i)
    {
        free(runner->shader_source[i]);
    }
    free(shader_source);
    for (i = 0; i < ARRAY_SIZE(runner->uniform_buffers); ++i)
    {
        free(runner->uniform_buffers[i].elements);
    }
    for (i = 0; i < runner->resource_count; ++i)
    {
        if (runner->resources[i])
            runner->ops->destroy_resource(runner, runner->resources[i]);
    }
}

static void read_shader_model_list(const char **line, uint32_t *model_mask)
{
    size_t len, i;

    *model_mask = 0;

    while (isspace(**line))
        ++*line;
    if (**line != '(')
        fatal_error("Malformed shader model list '%s'.\n", *line);
    ++*line;

    while (*line)
    {
        while (isspace(**line))
            ++*line;

        if (**line == ')')
        {
            ++*line;
            break;
        }

        for (i = 0; i < ARRAY_SIZE(sm_strings_dot); ++i)
        {
            len = strlen(sm_strings_dot[i]);
            if (strncmp(*line, sm_strings_dot[i], len))
                continue;

            bitmap_set(model_mask, i);
            *line += len;
            break;
        }

        if (i >= ARRAY_SIZE(sm_strings_dot))
            fatal_error("Unknown shader model '%s'.\n", *line);
    }
}


static void parse_header_directive(const char *line, uint32_t *test_model_mask)
{
    if (match_string(line, "test-shader-models", &line))
        read_shader_model_list(&line, test_model_mask);
    else
        fatal_error("Unknown header directive '%s'.\n", line);
}

static void parse_header(FILE *f, uint32_t *test_model_mask)
{
    enum parse_state state = STATE_NONE;
    char line_buffer[256];

    for (;;)
    {
        char *ret = fgets(line_buffer, sizeof(line_buffer), f);
        const char *line = line_buffer;

        if (!ret)
            break;

        if (!strcmp(line, "[header]\n"))
        {
            state = STATE_HEADER;
        }
        else if (line[0] == '[')
        {
            break;
        }
        else if (line[0] != '%' && line[0] != '\n')
        {
            if (state == STATE_HEADER)
                parse_header_directive(line, test_model_mask);
            else
                fatal_error("Malformed line '%s'.\n", line);
        }
    }
}

void run_shader_tests(struct shader_runner *runner, const struct shader_runner_caps *caps,
        const struct shader_runner_ops *ops, void *dxc_compiler)
{
    enum shader_model minimum_shader_model, maximum_shader_model;
    uint32_t test_model_mask = 0;
    FILE *f;

    if (!shader_test_options.filename)
        fatal_error("No filename specified.\n");

    if (!(f = fopen(shader_test_options.filename, "r")))
        fatal_error("Unable to open '%s' for reading: %s\n",
                shader_test_options.filename, strerror(errno));

    minimum_shader_model = max(caps->minimum_shader_model, shader_test_options.minimum_shader_model);
    maximum_shader_model = min(caps->maximum_shader_model, shader_test_options.maximum_shader_model);

    trace("Compiling SM%s-SM%s shaders with %s and executing with %s.\n",
            sm_strings_dot[minimum_shader_model], sm_strings_dot[maximum_shader_model],
            caps->compiler, caps->runner);
    trace_tags(caps);
    trace_shader_caps(caps->shader_caps);
    trace_format_cap(caps, FORMAT_CAP_UAV_LOAD, "uav-load");

    parse_header(f, &test_model_mask);

    /* Compilation-only tests could test more models than the ones specified in test_model_mask,
     * so we'll need to iterate over all possible shader models here. */
    for (enum shader_model model = minimum_shader_model; model <= maximum_shader_model; ++model)
    {
        if (fseek(f, 0, SEEK_SET))
            fatal_error("Failed to rewind file: %s\n", strerror(errno));

        memset(runner, 0, sizeof(*runner));
        runner->ops = ops;
        runner->caps = caps;
        runner->dxc_compiler = dxc_compiler;
        runner->minimum_shader_model = minimum_shader_model;
        runner->maximum_shader_model = maximum_shader_model;
        runner->shader_model = model;
        runner->test_model_mask = test_model_mask;

        run_shader_tests_for_model(runner, f);
    }

    fclose(f);
}

#ifdef _WIN32
static void print_dll_version(const char *file_name)
{
    BOOL (WINAPI *GetFileVersionInfoA)(const char *, DWORD, DWORD, void *);
    BOOL (WINAPI *VerQueryValueA)(void *, char *, void **, UINT*);
    DWORD (WINAPI *GetFileVersionInfoSizeA)(const char *, DWORD *);
    HMODULE version_module;
    DWORD size, handle;
    bool done = false;

    version_module = LoadLibraryA("version.dll");
    if (!version_module)
        goto out;

#define X(name) name = (void *)GetProcAddress(version_module, #name);
    X(GetFileVersionInfoSizeA);
    X(GetFileVersionInfoA);
    X(VerQueryValueA);
#undef X

    if (!GetFileVersionInfoSizeA || !GetFileVersionInfoA || !VerQueryValueA)
    {
        FreeLibrary(version_module);
        goto out;
    }

    size = GetFileVersionInfoSizeA(file_name, &handle);
    if (size)
    {
        char *data = malloc(size);

        if (GetFileVersionInfoA(file_name, handle, size, data))
        {
            VS_FIXEDFILEINFO *info;
            UINT len;

            if (VerQueryValueA(data, "\\", (void **)&info, &len))
            {
                trace("%s version: %lu.%lu.%lu.%lu\n", file_name,
                        info->dwFileVersionMS >> 16, info->dwFileVersionMS & 0xffff,
                        info->dwFileVersionLS >> 16, info->dwFileVersionLS & 0xffff);
                done = true;
            }
        }
        free(data);
    }

    FreeLibrary(version_module);

out:
    if (!done)
        trace("%s version: unknown\n", file_name);
}
#endif

static void run_compile_tests(void *dxc_compiler)
{
    struct shader_runner_caps caps = {0};
    struct shader_runner runner = {0};

    caps.runner = "hlsl";
    caps.compiler = dxc_compiler ? "dxcompiler" : HLSL_COMPILER;
    caps.minimum_shader_model = SHADER_MODEL_MIN;
    caps.maximum_shader_model = dxc_compiler ? SHADER_MODEL_MAX : SHADER_MODEL_5_1;
    for (unsigned int i = 0; i < SHADER_CAP_COUNT; ++i)
        caps.shader_caps[i] = true;
    for (unsigned int i = 0; i < DXGI_FORMAT_COUNT; ++i)
        caps.format_caps[i] = ~0u;

    if (test_skipping_execution(caps.runner, caps.compiler,
            caps.minimum_shader_model, caps.maximum_shader_model))
        return;

    run_shader_tests(&runner, &caps, NULL, dxc_compiler);
}

static bool parse_shader_model(const char *str, enum shader_model *sm)
{
    for (enum shader_model model = SHADER_MODEL_MIN; model <= SHADER_MODEL_MAX; ++model)
    {
        if (!strcmp(str, sm_strings_dot[model]))
        {
            *sm = model;
            return true;
        }
    }
    trace("Ignoring invalid shader model string '%s'\n", str);
    return false;
}

static inline void parse_shader_test_args(int argc, char **argv)
{
    unsigned int i;

    shader_test_options.minimum_shader_model = SHADER_MODEL_MIN;
    shader_test_options.maximum_shader_model = SHADER_MODEL_MAX;

    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--sm-min"))
            parse_shader_model(argv[++i], &shader_test_options.minimum_shader_model);
        else if (!strcmp(argv[i], "--sm-max"))
            parse_shader_model(argv[++i], &shader_test_options.maximum_shader_model);
        else if (!strcmp(argv[i], "--filter-executor"))
            shader_test_options.executor_filter = argv[++i];
        else if (!strcmp(argv[i], "--filter-compiler"))
            shader_test_options.compiler_filter = argv[++i];
        else if (argv[i][0] != '-')
            shader_test_options.filename = argv[i];
    }
}

START_TEST(shader_runner)
{
    IDxcCompiler3 *dxc;

    parse_args(argc, argv);
    parse_shader_test_args(argc, argv);

    if (shader_test_options.minimum_shader_model != SHADER_MODEL_MIN
            || shader_test_options.maximum_shader_model != SHADER_MODEL_MAX)
    {
        trace("Running shader models where %s <= SM <= %s\n",
                sm_strings_dot[shader_test_options.minimum_shader_model],
                sm_strings_dot[shader_test_options.maximum_shader_model]);
    }

    if (shader_test_options.executor_filter)
        trace("Running shaders with executor %s\n", shader_test_options.executor_filter);

    if (shader_test_options.compiler_filter)
        trace("Running shaders with compiler %s\n", shader_test_options.compiler_filter);

    dxc = dxcompiler_create();

    trace("Running HLSL compile tests.\n");
    run_compile_tests(dxc);

#if defined(VKD3D_CROSSTEST)
    trace("Running tests from a Windows cross build\n");

    run_shader_tests_d3d9();
    run_shader_tests_d3d11();
    run_shader_tests_d3d12(dxc);

    if (dxc)
        print_dll_version("dxcompiler.dll");
    print_dll_version("d3dcompiler_47.dll");
    print_dll_version("dxgi.dll");
    print_dll_version("d3d9.dll");
    print_dll_version("d3d11.dll");
    print_dll_version("d3d12.dll");
    print_dll_version("d3d12core.dll");
    print_dll_version("d3d10warp.dll");
    if (test_options.enable_debug_layer)
        print_dll_version("d3d12sdklayers.dll");

#elif defined(_WIN32)
    trace("Running tests from a Windows non-cross build\n");

    run_shader_tests_d3d9();
    run_shader_tests_d3d11();
    run_shader_tests_d3d12(dxc);

    if (dxc)
        print_dll_version(SONAME_LIBDXCOMPILER);
    print_dll_version("d3d9.dll");
    print_dll_version("d3d11.dll");

#else
    trace("Running tests from a Unix build\n");

# ifdef HAVE_OPENGL
    run_shader_tests_gl();
# endif
# ifdef HAVE_METAL
    run_shader_tests_metal(dxc);
# endif
    run_shader_tests_vulkan(dxc);
    run_shader_tests_d3d12(dxc);
#endif

    if (dxc)
        IDxcCompiler3_Release(dxc);
}
