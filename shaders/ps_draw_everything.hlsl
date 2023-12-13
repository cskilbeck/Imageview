#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    float4 pixel = border_color;

    int2 p = input.position.xy;
    uint anim1 = (uint) (p.x + p.y + frame);
    uint anim2 = anim1 - top_left.x - top_left.y;

    // checkerboard / image

    if (p.x > top_left.x && p.y > top_left.y && p.x < bottom_right.x && p.y < bottom_right.y)
    {
        float4 background = 0;
        float2 uv = input.texcoord * uv_scale + uv_offset;
        pixel = mytexture.Sample(mysampler, uv);

        int2 xy = floor(fmod((p + grid_offset) / grid_size, 2));
        background = checkerboard_color[xy.x + (xy.y * 2)];

        pixel = lerp(background, pixel, pixel.a);
    }

    // selection

    if (p.x > inner_select_rect.x && p.y > inner_select_rect.y && p.x < inner_select_rect.z && p.y < inner_select_rect.w)
    {
        pixel = lerp(pixel, select_color, select_color.a);
    }
    else if (p.x > outer_select_rect.x && p.y > outer_select_rect.y && p.x < outer_select_rect.z && p.y < outer_select_rect.w)
    {
        float4 overlay = select_outline_color[(anim2 / dash_length) % 2];
        pixel = lerp(pixel, overlay, overlay.a);
    }

    // crosshairs

    if (any(abs(p - crosshairs) < crosshair_width))
    {
        float4 xhair = crosshair_color[(anim1 / crosshair_dash_length) % 2];
        pixel = lerp(pixel, xhair, xhair.a);
    }

    return pixel;
}
