#include "hlsl.h"

vs_out main(uint id : SV_VertexID)
{
    vs_out output;

    float2 pos = float2(id & 1, (id >> 1) & 1);
    output.texcoord = mul(texture_transform, float4(pos.x, pos.y, 0, 1)).xy;
    output.position = float4(rect.xy + pos * rect.zw, 0, 1);

    return output;
}
