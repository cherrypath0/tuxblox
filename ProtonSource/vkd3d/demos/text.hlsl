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

cbuffer text_cb : register(b0)
{
    uint4 screen_size;
    uint4 glyphs[96];
};

Buffer<uint> text : register(t0);

struct text_run
{
    float4 colour : COLOUR;
    uint2 position : POSITION;
    uint start_idx : IDX;           /* The start offset of the run in the "text" Buffer. */
    uint char_count : COUNT;
    uint reverse : REVERSE;
    float scale : SCALE;
};

struct ps_in
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    uint start_idx : IDX;           /* The start offset of the run in the "text" Buffer. */
    uint reverse : REVERSE;
    float4 colour : COLOUR;
};

struct ps_in vs_main(struct text_run t, uint id : SV_VertexID)
{
    float2 pixel, pos, size;
    struct ps_in o;

    pixel = float2(2.0, 2.0) / float2(screen_size.x, screen_size.y);
    pos = pixel * t.position - float2(1.0, 1.0);
    size = pixel * t.scale * float2(t.char_count * 9.0, 16.0);

    o.position.x = (id & 0x1) * size.x + pos.x;
    o.position.y = ((id >> 1) & 0x1) * size.y + pos.y;
    o.position.z = 0.0;
    o.position.w = 1.0;

    o.texcoord.x = (id & 0x1) * t.char_count;
    o.texcoord.y = (~id >> 1) & 0x1;

    o.start_idx = t.start_idx;
    o.reverse = t.reverse;
    o.colour = t.colour;

    return o;
}

float4 ps_main(struct ps_in i) : SV_TARGET
{
    uint idx, glyph_id, row;
    uint4 glyph;
    uint2 texel;

    /* We determine the current character based on the start offset and texture
     * coordinate. We then lookup the corresponding glyph in glyphs[]. */
    idx = i.start_idx + i.texcoord.x;
    glyph_id = text[idx] - 0x20;
    glyph = glyphs[glyph_id];

    /* Find the row within the glyph bitmap, and then the pixel within that row.
     * Note that we apply dot stretching here; a single pixel in the source
     * glyph results in two pixels in the output. */
    texel = frac(i.texcoord.xy) * float2(9, 16);
    row = (glyph[texel.y / 4] >> (8 * (texel.y % 4))) & 0xff;
    if (!(i.reverse ^ (((row | (row << 1)) >> (8 - texel.x)) & 0x1)))
        discard;

    /* Scan line gaps. */
    if (uint(i.position.y) & 1)
        return float4(i.colour.xyz * (screen_size.z >= 2 ? 0.5 : 0.8), 1.0);

    return i.colour;
}
