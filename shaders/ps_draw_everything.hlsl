#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float in_rect(float2 p, float4 rect)
{
    return all(float4(p.x > rect.x, p.y > rect.y, p.x < rect.z, p.y < rect.w));
}

float4 main(vs_out input) : SV_TARGET
{
    float2 p = input.position.xy;

    // checkerboard / image

    float tl = in_rect(p, image_rect);
    float2 uv = input.texcoord * uv_scale + uv_offset;
    float4 pixel = mytexture.Sample(mysampler, uv);
    int2 xy = int2((p.xy + checkerboard_offset) * checkerboard_size) & int2(1, 1);
    float4 check = checkerboard_color[xy.x + (xy.y * 2)];
    float4 background = lerp(border_color, check, tl);
    pixel = lerp(background, pixel, pixel.a);

    // selection inner

    float inner = in_rect(p, inner_select_rect);
    pixel = lerp(pixel, select_color, select_color.a * inner);

    // selection rectangle

    float outer = in_rect(p, outer_select_rect) * (1 - inner);
    float sel_anim = (p.x + p.y - select_frame - image_rect.x - image_rect.y) * select_dash_length;
    float4 overlay = select_outline_color[int(sel_anim) & 1];
    pixel = lerp(pixel, overlay, overlay.a * outer);

    // crosshairs

    float cross = any(abs(p - crosshairs) < crosshair_width);
    float xhair_anim = (p.x + p.y - crosshair_frame) * crosshair_dash_length;
    float4 xhair = crosshair_color[int(xhair_anim) & 1];
    pixel = lerp(pixel, xhair, xhair.a * cross);

    return pixel;
}
