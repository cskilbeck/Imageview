#include "hlsl.h"

vs_out main(uint id : SV_VertexID)
{
    vs_out output;
    float3x3 tt =
    {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    float2 pos = float2(id & 1, (id >> 1) & 1);
    //output.texcoord = mul(tt, float3(pos.x, pos.y, 1)).xy;
    output.texcoord = mul(texture_transform, float4(pos.x, pos.y, 0, 1)).xy;
    output.position = float4(pos * rect.zw + rect.xy, 0, 1);

    return output;
}
