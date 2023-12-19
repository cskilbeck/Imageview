cbuffer constants{
#include "../src/shader_constants.h"
};
struct vs_out
{
    float4 position : SV_POSITION;
    float2 texcoord : TEX;
};
