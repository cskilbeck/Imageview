#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(vs_out input) : SV_TARGET
{
    float2 tc = input.texcoord * uv_scale + uv_offset;
    return mytexture.Sample(mysampler, tc);
}
