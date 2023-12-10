#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    int2 p = int2(input.position.x, input.position.y);

    float4 background = border_color;

    if (p.x >= top_left.x && p.y >= top_left.y && p.x < bottom_right.x && p.y < bottom_right.y)
    {
        int2 xy = (p + grid_offset) / grid_size % 2;
        background = grid_color[xy.x + (xy.y * 2)];;
    }

    float4 overlay = float4(0, 0, 0, 0);

    if (p.x >= inner_select_rect.x && p.y >= inner_select_rect.y && p.x < inner_select_rect.z && p.y < inner_select_rect.w)
    {
        overlay = select_color;
    }
    else if (p.x >= outer_select_rect.x && p.y >= outer_select_rect.y && p.x < outer_select_rect.z && p.y < outer_select_rect.w)
    {
        overlay = select_outline_color[((uint) (p.x + p.y + frame) / dash_length) % 2];
    }

    float4 pixel = mytexture.Sample(mysampler, input.texcoord * uv_scale + uv_offset);

    pixel = lerp(background, pixel, pixel.a);
    pixel = lerp(pixel, overlay, overlay.a);

    return pixel;
}
