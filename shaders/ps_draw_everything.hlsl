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
    float4 xhair = 0;
    float2 uv = input.texcoord * uv_scale + uv_offset;

    // for animating crosshairs and selection rectangle

    uint anim = (uint) (p.x + p.y + frame - top_left.x - top_left.y);

    // crosshairs

    float2 cross = floor(p - crosshairs);
    if (!all(cross))
    {
        xhair = crosshair_color[(anim / crosshair_dash_length) % 2];
    }

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
        overlay = select_outline_color[(anim / dash_length) % 2];
    }

    pixel = lerp(background, pixel, pixel.a);
    pixel = lerp(pixel, grid_col, grid_col.a);
    pixel = lerp(pixel, overlay, overlay.a);
    pixel = lerp(pixel, xhair, xhair.a);

    return pixel;
}
