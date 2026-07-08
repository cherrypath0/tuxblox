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

#define M_PI 3.14159265

cbuffer teapot_cb : register(b0)
{
    float4x4 mvp_matrix;
    float3 eye;
    float level;
    uint wireframe, flat;
};

struct control_point
{
    float4 position : SV_POSITION;
};

struct patch_constant_data
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct gs_in
{
    float4 position : SV_POSITION;
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct ps_in
{
    float4 position : SV_POSITION;
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 barycentric : BARYCENTRIC;
};

float4 vs_main(float4 position : POSITION, uint id : SV_InstanceID) : SV_POSITION
{
    /* Mirror/flip patches based on the instance ID. */
    position.w = -1.0;
    if (id & 1)
        position.yw = -position.yw;
    if (id & 2)
        position.xw = -position.xw;

    return position;
}

struct patch_constant_data patch_constant(InputPatch<control_point, 16> input)
{
    struct patch_constant_data output;

    output.edges[0] = level;
    output.edges[1] = level;
    output.edges[2] = level;
    output.edges[3] = level;
    output.inside[0] = level;
    output.inside[1] = level;

    return output;
}

[domain("quad")]
[outputcontrolpoints(16)]
[outputtopology("triangle_ccw")]
[partitioning("integer")]
[patchconstantfunc("patch_constant")]
struct control_point hs_main(InputPatch<control_point, 16> input, uint i : SV_OutputControlPointID)
{
    /* Reorder mirrored/flipped control points. */
    if (input[0].position.w < 0.0)
    {
        uint u = i % 4, v = i / 4;
        return input[v * 4 + (3 - u)];
    }

    return input[i];
}

float3 eval_quadratic(float3 p0, float3 p1, float3 p2, float t)
{
    return lerp(lerp(p0, p1, t), lerp(p1, p2, t), t);
}

float3 eval_cubic(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    return lerp(eval_quadratic(p0, p1, p2, t),
            eval_quadratic(p1, p2, p3, t), t);
}

struct gs_in eval_patch(float2 t, float4 p[16])
{
    float3 position, normal, q[4], u, v;
    struct gs_in o;

    q[0] = eval_cubic( p[0].xyz,  p[1].xyz,  p[2].xyz,  p[3].xyz, t.x);
    q[1] = eval_cubic( p[4].xyz,  p[5].xyz,  p[6].xyz,  p[7].xyz, t.x);
    q[2] = eval_cubic( p[8].xyz,  p[9].xyz, p[10].xyz, p[11].xyz, t.x);
    q[3] = eval_cubic(p[12].xyz, p[13].xyz, p[14].xyz, p[15].xyz, t.x);
    u = eval_quadratic(q[0], q[1], q[2], t.y) - eval_quadratic(q[1], q[2], q[3], t.y);

    q[0] = eval_cubic(p[0].xyz, p[4].xyz,  p[8].xyz, p[12].xyz, t.y);
    q[1] = eval_cubic(p[1].xyz, p[5].xyz,  p[9].xyz, p[13].xyz, t.y);
    q[2] = eval_cubic(p[2].xyz, p[6].xyz, p[10].xyz, p[14].xyz, t.y);
    q[3] = eval_cubic(p[3].xyz, p[7].xyz, p[11].xyz, p[15].xyz, t.y);
    v = eval_quadratic(q[0], q[1], q[2], t.x) - eval_quadratic(q[1], q[2], q[3], t.x);

    position = eval_cubic(q[0], q[1], q[2], q[3], t.x);
    o.position = mul(mvp_matrix, float4(position, 1.0));
    o.pos = position;

    /* The patches for the bottom of the teapot and the top of its lid are
     * degenerate. Technically this isn't the right way to deal with that, but
     * it's easy and gets the right result for these patches. */
    if (length(v) == 0.0)
        normal = cross(p[4].xyz - p[0].xyz, p[7].xyz - p[3].xyz);
    else
        normal = cross(u, v);
    o.normal = normalize(normal);

    return o;
}

[domain("quad")]
struct gs_in ds_main(struct patch_constant_data input,
        float2 tess_coord : SV_DomainLocation, const OutputPatch<control_point, 16> patch)
{
    return eval_patch(tess_coord, patch);
}

[maxvertexcount(3)]
void gs_main(triangle struct gs_in i[3], inout TriangleStream<struct ps_in> stream)
{
    struct ps_in v[3];
    float3 n;

    v[0].position = i[0].position;
    v[0].pos = i[0].pos;
    v[0].normal = i[0].normal;
    v[0].barycentric = float2(1.0, 0.0);

    v[1].position = i[1].position;
    v[1].pos = i[1].pos;
    v[1].normal = i[1].normal;
    v[1].barycentric = float2(0.0, 1.0);

    v[2].position = i[2].position;
    v[2].pos = i[2].pos;
    v[2].normal = i[2].normal;
    v[2].barycentric = float2(0.0, 0.0);

    if (flat)
    {
        n = normalize(cross(i[1].pos - i[0].pos, i[2].pos - i[0].pos));
        v[0].normal = n;
        v[1].normal = n;
        v[2].normal = n;
    }

    stream.Append(v[0]);
    stream.Append(v[1]);
    stream.Append(v[2]);
}

/* Lambertian diffuse. */
float3 brdf_lambert(float3 diffuse)
{
    return diffuse / M_PI;
}

/* The Schlick Fresnel approximation:
 *
 *     R(θ) ≈ R₀ + (1 - R₀)(1 - c̅o̅s̅ θ)⁵
 */
float3 fresnel_schlick(float3 r0, float cos_theta)
{
    return lerp(r0, 1.0, pow(1.0 - cos_theta, 5.0));
}

float g1(float cos_theta, float alpha_sq)
{
    return cos_theta + sqrt(alpha_sq + (cos_theta - alpha_sq * cos_theta) * cos_theta);
}

/* Trowbridge-Reitz, "Average irregularity representation of a rough surface for ray reflection".
 * Also known as "GGX".
 *
 *     G1(θ) = 2 / (1 + sqrt(α² + (1 - α²)c̅o̅s̅² θ))
 *     G(θᵢ, θₒ) = G1(θᵢ) * G1(θₒ)
 *
 * This returns G / (4 c̅o̅s̅ θᵢ c̅o̅s̅ θₒ)
 */
float geometric_att_trowbridge_reitz(float cos_theta_i, float cos_theta_o, float alpha_sq)
{
    return 1.0 / (g1(cos_theta_i, alpha_sq) * g1(cos_theta_o, alpha_sq));
}

/* Trowbridge-Reitz, "Average irregularity representation of a rough surface for ray reflection".
 * Also known as "GGX".
 *
 *     D(θ) = α² / π((cos² θ)(α² - 1) + 1)²
 */
float ndf_trowbridge_reitz(float cos_theta_h, float alpha_sq)
{
    float f = (cos_theta_h * alpha_sq - cos_theta_h) * cos_theta_h + 1.0;
    return alpha_sq / (M_PI * f * f);
}

float4 ps_main(struct ps_in i) : SV_TARGET
{
    float3 barycentric, diffuse, diffuse_colour, radiance, specular, f, h, n, v;
    float alpha, alpha_sq, cos_theta_h, cos_theta_i, cos_theta_o, d, g, wire;
    float3 light_dir = normalize(float3(5.0, 5.0, 10.0));
    float3 light_colour = float3(1.0, 0.95, 0.88);
    float3 light_irradiance = 5.0 * light_colour;
    float3 base_colour = float3(0.8, 0.8, 0.8);
    float3 f0 = float3(0.04, 0.04, 0.04);
    float3 ambient = 0.3 * light_colour;
    float roughness = 0.2;
    float metallic = 0.3;

    n = normalize(i.normal);
    v = normalize(eye - i.pos);
    h = normalize(light_dir + v);
    cos_theta_h = dot(n, h);
    cos_theta_i = saturate(dot(n, light_dir));
    cos_theta_o = saturate(dot(n, v));

    diffuse_colour = base_colour * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - metallic);
    alpha = roughness * roughness;
    alpha_sq = alpha * alpha;

    /* Cook-Torrance. The division by (4 c̅o̅s̅ θᵢ c̅o̅s̅ θₒ) is folded into G. */
    f = fresnel_schlick(lerp(f0, base_colour, metallic), dot(v, h));
    g = geometric_att_trowbridge_reitz(cos_theta_i, cos_theta_o, alpha_sq);
    d = ndf_trowbridge_reitz(cos_theta_h, alpha_sq);
    diffuse = (1.0 - f) * brdf_lambert(diffuse_colour);
    specular = f * g * d;
    radiance = (diffuse + specular) * light_irradiance * cos_theta_i;
    radiance += ambient * base_colour;

    barycentric = float3(i.barycentric, 1.0 - (i.barycentric.x + i.barycentric.y));
    barycentric /= fwidth(barycentric);
    wire = wireframe ? min(min(barycentric.x, barycentric.y), barycentric.z) : 1.0;

    return float4(lerp(float3(1.00, 0.69, 0.0), saturate(radiance), saturate(wire)), 1.0);
}
