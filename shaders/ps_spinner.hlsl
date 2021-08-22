#include "hlsl.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float length_squared(float2 a, float2 b)
{
    float2 d = b - a;
    return dot(d, d);
}

float distance(float2 a, float2 b)
{
    return sqrt(length_squared(a, b));
}

float minimum_distance(float2 s, float2 e, float2 p)
{
    float2 d = e - s;
    float l = dot(d, d);
    float t = clamp(dot(p - s, d) / l, 0, 1);
    float2 q = s + t * d;
    return distance(p, q);
}

float4 main(vs_out input) : SV_TARGET
{
    float x = 1 - min(minimum_distance(glowing_line_s, glowing_line_e, input.position.xy) / 12, 1);
    return float4(x, x, x, x);
}

