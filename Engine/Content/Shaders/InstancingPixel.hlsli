#ifndef ENGINE_INSTANCING_PIXEL_HLSLI
#define ENGINE_INSTANCING_PIXEL_HLSLI

#if !defined(SHADOW_PASS)

static const float UnitSpecularWeight = 0.35f;
static const float UnitAmbientModel = 1.0f;

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float fade : TEXCOORD2;
    float4 color : COLOR;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 SampleBaseColor(PS_IN input)
{
    return HasTexture > 0.5f ? tex.Sample(samp, input.texCoord) * input.color : input.color;
}

#if defined(DEFERRED_GBUFFER)
GBufferOut PSMain(PS_IN input)
{
    ClipByFade(input.fade, input.pos.xy);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    return EncodeGBuffer(baseColor.rgb, input.normal, input.worldPos, material);
}
#else
float4 PSMain(PS_IN input) : SV_Target
{
    ClipByFade(input.fade, input.pos.xy);
    float4 baseColor = SampleBaseColor(input);
    MaterialLighting material = MakeMaterialLighting(UnitSpecularWeight, UnitAmbientModel);
    float3 litColor = ComputeSceneLighting(baseColor.rgb, normalize(input.normal), input.worldPos, material);
    return float4(saturate(litColor), baseColor.a);
}
#endif

#endif // !defined(SHADOW_PASS)

#endif // ENGINE_INSTANCING_PIXEL_HLSLI
