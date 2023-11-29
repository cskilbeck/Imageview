#include "hlsl.h"

float4 main(vs_out input) : SV_TARGET
{
    float2 xy = fmod(floor((input.position.xy + grid_offset) / grid_size), 2);
    return grid_color[xy.x + (xy.y * 2)];
}
