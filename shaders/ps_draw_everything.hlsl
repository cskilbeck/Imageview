#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    int2 p = input.position.xy;

    // checkerboard

    float4 background = border_color;
    float4 grid_col = 0;
    float4 overlay = 0;
    float4 pixel = 0;
    float2 uv = input.texcoord * uv_scale + uv_offset;

    // checkerboard

    if (p.x > top_left.x && p.y > top_left.y && p.x < bottom_right.x && p.y < bottom_right.y)
    {
        pixel = mytexture.Sample(mysampler, uv);

        int2 xy = floor(fmod((p + grid_offset) / grid_size, 2));
        background = grid_color[xy.x + (xy.y * 2)];;
    }

    // selection

    if (p.x > inner_select_rect.x && p.y > inner_select_rect.y && p.x < inner_select_rect.z && p.y < inner_select_rect.w)
    {
        overlay = select_color;
    }
    else if (p.x > outer_select_rect.x && p.y > outer_select_rect.y && p.x < outer_select_rect.z && p.y < outer_select_rect.w)
    {
        overlay = select_outline_color[((uint) (p.x + p.y + frame - top_left.x - top_left.y) / dash_length) % 2];
    }

    pixel = lerp(background, pixel, pixel.a);
    pixel = lerp(pixel, grid_col, grid_col.a);
    pixel = lerp(pixel, overlay, overlay.a);

    return pixel;
}
