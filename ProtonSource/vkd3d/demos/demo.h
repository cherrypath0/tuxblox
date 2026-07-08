/*
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

/* Hack for MinGW-w64 headers.
 *
 * We want to use WIDL C inline wrappers because some methods
 * in D3D12 interfaces return aggregate objects. Unfortunately,
 * WIDL C inline wrappers are broken when used with MinGW-w64
 * headers because FORCEINLINE expands to extern inline
 * which leads to the "multiple storage classes in declaration
 * specifiers" compiler error.
 */
#ifdef __MINGW32__
#include <_mingw.h>
# ifdef __MINGW64_VERSION_MAJOR
#  undef __forceinline
#  define __forceinline __inline__ __attribute__((__always_inline__,__gnu_inline__))
# endif
#endif

#include <vkd3d_windows.h>
#define WIDL_C_INLINE_WRAPPERS
#define COBJMACROS
#include <vkd3d_d3d12.h>
#include <inttypes.h>
#include <math.h>

#ifdef __WIN32__
#define DEMO_ASM_PUSHSECTION ".section rdata\n\t"
#define DEMO_ASM_POPSECTION ".text\n\t"
#define DEMO_ASM_OBJECT_TYPE(name)
#elif defined(__APPLE__)
#define DEMO_ASM_PUSHSECTION ".pushsection __TEXT,__const\n\t"
#define DEMO_ASM_POPSECTION ".popsection\n\t"
#define DEMO_ASM_OBJECT_TYPE(name)
#else
#define DEMO_ASM_PUSHSECTION ".pushsection .rodata\n\t"
#define DEMO_ASM_POPSECTION ".popsection\n\t"
#define DEMO_ASM_OBJECT_TYPE(name) ".type "name", @object\n\t"
#endif

#if (defined(__WIN32__) && defined(__i386__)) || defined(__APPLE__)
#define DEMO_ASM_NAME(name) "_"#name
#else
#define DEMO_ASM_NAME(name) #name
#endif

#define DEMO_EMBED_ASM(name, file) \
    DEMO_ASM_PUSHSECTION \
    ".global "name"\n\t" \
    DEMO_ASM_OBJECT_TYPE(name) \
    ".balign 8\n\t" \
    name":\n\t" \
    ".incbin \""file"\"\n\t" \
    name"_end:\n\t" \
    ".global "name"_size\n\t" \
    DEMO_ASM_OBJECT_TYPE(name"_size") \
    ".balign 8\n\t" \
    name"_size:\n\t" \
    ".int "name"_end - "name"\n\t" \
    DEMO_ASM_POPSECTION

#define DEMO_EMBED(name, file) \
    extern const unsigned int name##_size; \
    extern const uint8_t name[]; \
    __asm__(DEMO_EMBED_ASM(DEMO_ASM_NAME(name), file))

#if defined(__GNUC__) || defined(__clang__)
# ifdef __MINGW_PRINTF_FORMAT
#  define DEMO_PRINTF_FUNC(fmt, args) __attribute__((format(__MINGW_PRINTF_FORMAT, fmt, args)))
# else
#  define DEMO_PRINTF_FUNC(fmt, args) __attribute__((format(printf, fmt, args)))
# endif
#else
# define DEMO_PRINTF_FUNC(fmt, args)
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

#define DEMO_KEY_UNKNOWN        0x0000
#define DEMO_KEY_ESCAPE         0xff1b
#define DEMO_KEY_LEFT           0xff51
#define DEMO_KEY_UP             0xff52
#define DEMO_KEY_RIGHT          0xff53
#define DEMO_KEY_DOWN           0xff54
#define DEMO_KEY_KP_ADD         0xffab
#define DEMO_KEY_KP_SUBTRACT    0xffad
#define DEMO_KEY_F1             0xffbe

struct demo_vec3
{
    float x, y, z;
};

struct demo_vec4
{
    float x, y, z, w;
};

struct demo_uvec2
{
    uint32_t x, y;
};

struct demo_uvec4
{
    uint32_t x, y, z, w;
};

struct demo_matrix
{
    float m[4][4];
};

struct demo_patch
{
    uint16_t p[4][4];
};

struct demo_swapchain_desc
{
    unsigned int width;
    unsigned int height;
    unsigned int buffer_count;
    DXGI_FORMAT format;
};

typedef uint32_t demo_key;

static inline void demo_vec3_set(struct demo_vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void demo_vec3_subtract(struct demo_vec3 *v, const struct demo_vec3 *a, const struct demo_vec3 *b)
{
    demo_vec3_set(v, a->x - b->x, a->y - b->y, a->z - b->z);
}

static inline float demo_vec3_dot(const struct demo_vec3 *a, const struct demo_vec3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float demo_vec3_length(const struct demo_vec3 *v)
{
    return sqrtf(demo_vec3_dot(v, v));
}

static inline void demo_vec3_scale(struct demo_vec3 *v, const struct demo_vec3 *a, float s)
{
    demo_vec3_set(v, a->x * s, a->y * s, a->z *s);
}

static inline void demo_vec3_normalise(struct demo_vec3 *v, const struct demo_vec3 *a)
{
    demo_vec3_scale(v, a, 1.0f / demo_vec3_length(a));
}

static inline void demo_vec3_cross(struct demo_vec3 *v, const struct demo_vec3 *a, const struct demo_vec3 *b)
{
    demo_vec3_set(v, a->y * b->z - a->z * b->y, a->z * b->x - a->x * b->z, a->x * b->y - a->y * b->x);
}

static inline void demo_vec4_set(struct demo_vec4 *v, float x, float y, float z, float w)
{
    v->x = x;
    v->y = y;
    v->z = z;
    v->w = w;
}

static inline void demo_matrix_look_at_rh(struct demo_matrix *m, const struct demo_vec3 *eye,
        const struct demo_vec3 *ref, const struct demo_vec3 *up)
{
    struct demo_vec3 f, s, t, u;

    demo_vec3_subtract(&f, eye, ref);
    demo_vec3_normalise(&f, &f);
    demo_vec3_cross(&s, up, &f);
    demo_vec3_normalise(&s, &s);
    demo_vec3_cross(&u, &f, &s);
    demo_vec3_set(&t, demo_vec3_dot(&s, eye), demo_vec3_dot(&u, eye), demo_vec3_dot(&f, eye));

    *m = (struct demo_matrix)
    {{
        { s.x,  u.x,  f.x, 0.0f},
        { s.y,  u.y,  f.y, 0.0f},
        { s.z,  u.z,  f.z, 0.0f},
        {-t.x, -t.y, -t.z, 1.0f},
    }};
}

static inline void demo_matrix_multiply(struct demo_matrix *out,
        const struct demo_matrix *a, const struct demo_matrix *b)
{
    unsigned int i, j;

    for (i = 0; i < 4; ++i)
    {
        for (j = 0; j < 4; ++j)
        {
            out->m[i][j] = a->m[i][0] * b->m[0][j]
                    + a->m[i][1] * b->m[1][j]
                    + a->m[i][2] * b->m[2][j]
                    + a->m[i][3] * b->m[3][j];
        }
    }
}

static inline void demo_matrix_perspective_rh(struct demo_matrix *m, float w, float h, float z_near, float z_far)
{
    float sx = 2.0 * z_near / w;
    float sy = 2.0 * z_near / h;
    float sz = z_far / (z_near - z_far);
    float d = z_near * sz;

    *m = (struct demo_matrix)
    {{
        {  sx, 0.0f, 0.0f,  0.0f},
        {0.0f,   sy, 0.0f,  0.0f},
        {0.0f, 0.0f,   sz, -1.0f},
        {0.0f, 0.0f,    d,  0.0f},
    }};
}

static inline void demo_rasterizer_desc_init_default(D3D12_RASTERIZER_DESC *desc)
{
    desc->FillMode = D3D12_FILL_MODE_SOLID;
    desc->CullMode = D3D12_CULL_MODE_BACK;
    desc->FrontCounterClockwise = FALSE;
    desc->DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc->DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc->SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc->DepthClipEnable = TRUE;
    desc->MultisampleEnable = FALSE;
    desc->AntialiasedLineEnable = FALSE;
    desc->ForcedSampleCount = 0;
    desc->ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

static inline void demo_blend_desc_init_default(D3D12_BLEND_DESC *desc)
{
    static const D3D12_RENDER_TARGET_BLEND_DESC rt_blend_desc =
    {
        .BlendEnable = FALSE,
        .LogicOpEnable = FALSE,
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ZERO,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ZERO,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .LogicOp = D3D12_LOGIC_OP_NOOP,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    unsigned int i;

    desc->AlphaToCoverageEnable = FALSE;
    desc->IndependentBlendEnable = FALSE;
    for (i = 0; i < ARRAY_SIZE(desc->RenderTarget); ++i)
    {
        desc->RenderTarget[i] = rt_blend_desc;
    }
}

static inline HRESULT demo_create_root_signature(ID3D12Device *device,
        const D3D12_ROOT_SIGNATURE_DESC *desc, ID3D12RootSignature **signature)
{
    ID3DBlob *blob;
    HRESULT hr;

    if (FAILED(hr = D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL)))
        return hr;
    hr = ID3D12Device_CreateRootSignature(device, 0, ID3D10Blob_GetBufferPointer(blob),
            ID3D10Blob_GetBufferSize(blob), &IID_ID3D12RootSignature, (void **)signature);
    ID3D10Blob_Release(blob);

    return hr;
}

#ifdef VKD3D_CROSSTEST
#include "demo_d3d12.h"
#else
# ifndef _WIN32
#  define INFINITE VKD3D_INFINITE
# endif
#include "demo_vkd3d.h"
#endif
