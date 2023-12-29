#include "hlsl.h"

float4 main(vs_out input) : SV_TARGET
{
    float2 p = input.position.xy;
    return colors[int((p.x + p.y - frame) * check_strip_size) & 1];
}
