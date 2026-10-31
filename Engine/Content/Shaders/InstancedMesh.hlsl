#include "Common.hlsli"
#include "Instancing.hlsli"
#include "InstancingPixel.hlsli"

StructuredBuffer<InstData> InstBuf : register(t0);

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

PS_IN VSMain(VS_IN input, uint instanceId : SV_InstanceID)
{
    PS_IN output = (PS_IN)0;

    InstData instance = InstBuf[instanceId];
    float4 worldPos = mul(input.pos, instance.World);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    output.worldPos = worldPos.xyz;
    // Instances use a uniform scale, so the instance matrix also transforms normals correctly.
    output.normal = normalize(mul(input.normal, (float3x3)instance.World));
    output.texCoord = input.texCoord;
    output.color = ObjectColor * instance.Color;
    output.fade = instance.Params.x;
    return output;
}

