#include "hlsl.h"

float4 main(vs_out input) : SV_TARGET
{
    int x = (int)input.position.x & 1;
    int y = (int)input.position.y & 1;
    return line_color[x + (y * 2)];
}