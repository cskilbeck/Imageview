#include "hlsl.h"

vs_out main(uint id : SV_VertexID)
{
    vs_out output;

    output.texcoord = float2(id & 1, (id >> 1) & 1);
    output.position = float4(output.texcoord * float2(2, -2) + float2(-1, 1), 0, 1);

    return output;
}
