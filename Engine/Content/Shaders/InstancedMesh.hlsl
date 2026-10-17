#include "Common.hlsli"
#include "Instancing.hlsli"

// Material of mass units (vehicles): mesh vertex format, per-instance transform and colour.
// The same file provides the forward variant and the G-buffer variant, so thousands of units
// can be lit in screen space by the deferred pass.
static const float UnitSpecularWeight = 0.35f;
static const float UnitAmbientModel = 1.0f; // hemispherical ambient, same as the ground material

StructuredBuffer<InstData> InstBuf : register(t0);

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    // xy transition weight and direction, zw this unit's own phase in the dither pattern.
    float4 fade : TEXCOORD2;
    float4 color : COLOR;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

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
    output.fade = instance.Params;
    return output;
}

float4 SampleBaseColor(PS_IN input)
{
    return HasTexture > 0.5f ? tex.Sample(samp, input.texCoord) * input.color : input.color;
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    ClipByFade(input.fade.x, input.fade.y, input.pos.xy + input.fade.zw);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    return EncodeGBuffer(baseColor.rgb, input.normal, input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    ClipByFade(input.fade.x, input.fade.y, input.pos.xy + input.fade.zw);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    float3 litColor = ComputeSceneLighting(baseColor.rgb, normalize(input.normal), input.worldPos, material);
    return float4(saturate(litColor), baseColor.a);
}
#endif
