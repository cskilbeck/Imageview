#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    uint2 p = (int2) input.position.xy;
    uint4 a = abs(int4(p - rect_f.xy, p - rect_f.zw));

    int left = a.x < select_border_width;
    int top = !left && (a.y < select_border_width);
    int right = !(left || top) && (a.z < select_border_width);
    int bottom = !(left || top || right) && (a.w < select_border_width);

    int lb = left || bottom;
    int tr = top || right;

    int base = lb || tr;

    int d1 = lb ? 0 : -1;
    int d2 = tr ? 0 : 1;

    uint dash = ((p.x + p.y + frame * (d1 + d2)) / dash_length) % 2;

    // return select_color[base + (dash & base)];
    return float4(1, 1, 1, 1);
}
