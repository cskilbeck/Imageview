#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    float4 pixel = mytexture.Sample(mysampler, input.texcoord);
    int2 xy = int2((input.position.xy + checkerboard_offset) * check_strip_size) & int2(1, 1);
    float4 check = colors[xy.x + (xy.y * 2)];
    return lerp(check, pixel, pixel.a);
}
