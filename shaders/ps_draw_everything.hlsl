#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    float2 xy = fmod(floor((input.position.xy + grid_offset) / grid_size), 2);
    float4 checkerboard = grid_color[xy.x + (xy.y * 2)];

    float4 background = background_color;
    float4 overlay = float4(0, 0, 0, 0);

    float px = input.position.x;
    float py = input.position.y;

    int outline = ((px + py + frame) / dash_length) % 2;

    if (px >= top_left.x && py >= top_left.y && px < bottom_right.x && py < bottom_right.y)
    {
        background = checkerboard;
    }

    if (px >= inner_select_rect.x && py >= inner_select_rect.y && px < inner_select_rect.z && py < inner_select_rect.w)
    {
        overlay = select_color;
    }
    else if (px >= outer_select_rect.x && py >= outer_select_rect.y && px < outer_select_rect.z && py < outer_select_rect.w)
    {
        overlay = select_outline_color[outline];
    }

    float4 pixel = mytexture.Sample(mysampler, input.texcoord * uv_scale + uv_offset);

    pixel = lerp(background, pixel, pixel.a);
    pixel = lerp(pixel, overlay, overlay.a);

    return pixel;
}
