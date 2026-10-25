#include "MaterialCommon.hlsli"
// The engine maps this include to the surface file of the material being compiled.
#include "MaterialSurface.hlsli"

// Shader of every material asset: VS for VertexFormat::Mesh plus forward and G-buffer pixel
// shaders around the Surface function. The G-buffer variant exists only for lit opaque/masked
// materials without emissive; for the others DEFERRED_GBUFFER compiles the forward shader
// (one render target), and the renderer draws them in its forward-only pass.
//
// Pass variants (set by the engine):
//     MATERIAL_INSTANCED       transform, colour and data of each copy come from the instance stream
//     MATERIAL_SHADOW          depth of a masked material for a shadow map (clip applied)
//     MATERIAL_IMPOSTOR_BAKE   albedo and object-space normal for one tile of an impostor atlas

#if !defined(MATERIAL_UNLIT) && !defined(MATERIAL_EMISSIVE) && MATERIAL_BLEND <= MATERIAL_BLEND_MASKED
#define MATERIAL_DEFERRED 1
#endif

static const float MaterialDefaultSpecularWeight = 0.25f;
static const float MaterialDefaultAmbientModel = 1.0f;

#if defined(MATERIAL_INSTANCED)
#include "../Instancing.hlsli"
StructuredBuffer<InstData> InstBuf : register(t0); // vertex stage
#endif

#if defined(MATERIAL_SHADOW) || defined(MATERIAL_IMPOSTOR_BAKE)
// Object constants stay in b0 so Surface sees the usual inputs; the camera of the pass
// (light of a shadow cascade, view of an atlas tile) comes separately.
cbuffer MaterialPassTransform : register(b2)
{
    float4x4 PassViewProjection;
};
#endif

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float3 instanceData : TEXCOORD2;
    float fade : TEXCOORD3; // LOD cross-fade of the copy, 1 = fully visible
};

PS_IN VSMain(VS_IN input, uint instanceId : SV_InstanceID)
{
    PS_IN output = (PS_IN)0;
#if defined(MATERIAL_INSTANCED)
    InstData instance = InstBuf[instanceId];
    float4 worldPos = mul(input.pos, instance.World);
    // Copies use a uniform scale, so their matrix transforms normals correctly.
    output.normal = mul(input.normal, (float3x3)instance.World);
    output.color = ObjectColor * instance.Color;
    output.instanceData = instance.Params.yzw;
    output.fade = instance.Params.x;
#else
    float4 worldPos = mul(input.pos, worldMatrix);
    // Inverse-transpose keeps normals perpendicular under non-uniform scale.
    output.normal = mul(input.normal, (float3x3)normalMatrix);
    output.color = ObjectColor;
    output.fade = 1.0f;
#endif
#if defined(MATERIAL_SHADOW) || defined(MATERIAL_IMPOSTOR_BAKE)
    output.pos = mul(worldPos, PassViewProjection);
#else
    output.pos = mul(mul(worldPos, viewMatrix), projectionMatrix);
#endif
    output.worldPos = worldPos.xyz;
    output.uv = input.uv;
    return output;
}

MaterialOutput EvaluateMaterial(PS_IN input, bool frontFace)
{
#if defined(MATERIAL_INSTANCED)
    ClipByFade(input.fade, input.pos.xy);
#endif
    float3 normal = normalize(input.normal);
#if defined(MATERIAL_TWO_SIDED)
    normal = frontFace ? normal : -normal;
#endif

    MaterialInput i;
    i.WorldPos = input.worldPos;
    i.Normal = normal;
    i.UV = input.uv;
    i.ViewDir = normalize(CameraPosition.xyz - input.worldPos);
    i.Color = input.color;
    i.InstanceData = input.instanceData;
    i.Time = TimeParams.x;
    i.FrontFace = frontFace;

    MaterialOutput o;
    o.Albedo = input.color.rgb;
    o.Normal = normal;
    o.SpecularWeight = MaterialDefaultSpecularWeight;
    o.AmbientModel = MaterialDefaultAmbientModel;
    o.Emissive = float3(0.0f, 0.0f, 0.0f);
    o.Alpha = input.color.a;

    Surface(i, o);

    o.Normal = normalize(o.Normal);
#if MATERIAL_BLEND == MATERIAL_BLEND_MASKED
    clip(o.Alpha - MATERIAL_ALPHA_CLIP);
#endif
    return o;
}

float4 ShadeForward(MaterialOutput o, float3 worldPos)
{
#if defined(MATERIAL_UNLIT)
    float3 color = o.Albedo;
#else
    MaterialLighting lighting = MakeMaterialLighting(o.SpecularWeight, o.AmbientModel);
    float3 color = ComputeSceneLighting(o.Albedo, o.Normal, worldPos, lighting);
#endif
    color = saturate(color + o.Emissive);

#if MATERIAL_BLEND == MATERIAL_BLEND_TRANSLUCENT
    return float4(color, saturate(o.Alpha));
#elif MATERIAL_BLEND == MATERIAL_BLEND_ADDITIVE
    return float4(color * saturate(o.Alpha), 1.0f); // blended ONE + ONE
#else
    return float4(color, 1.0f);
#endif
}

#if defined(MATERIAL_SHADOW)
// Depth only: the pixel survives unless the surface clips it (masked materials).
void PSMain(PS_IN input, bool frontFace : SV_IsFrontFace)
{
    EvaluateMaterial(input, frontFace);
}
#elif defined(MATERIAL_IMPOSTOR_BAKE)
// Same layout as Shaders/ImpostorBake.hlsl. Lighting is applied when the sprite is drawn, the colour
// of each copy multiplies the albedo there; emissive and animation over time are not captured.
struct BakeOut
{
    float4 Albedo : SV_Target0; // rgb colour, a = coverage
    float4 Normal : SV_Target1; // object-space normal packed to 0..1
};

BakeOut PSMain(PS_IN input, bool frontFace : SV_IsFrontFace)
{
    MaterialOutput o = EvaluateMaterial(input, frontFace);
    BakeOut output;
    output.Albedo = float4(saturate(o.Albedo), 1.0f);
    output.Normal = float4(o.Normal * 0.5f + 0.5f, 1.0f);
    return output;
}
#elif defined(DEFERRED_GBUFFER) && defined(MATERIAL_DEFERRED)
GBufferOut PSMain(PS_IN input, bool frontFace : SV_IsFrontFace)
{
    MaterialOutput o = EvaluateMaterial(input, frontFace);
    MaterialLighting lighting = MakeMaterialLighting(o.SpecularWeight, o.AmbientModel);
    return EncodeGBuffer(o.Albedo, o.Normal, input.worldPos, lighting);
}
#else
float4 PSMain(PS_IN input, bool frontFace : SV_IsFrontFace) : SV_Target
{
    return ShadeForward(EvaluateMaterial(input, frontFace), input.worldPos);
}
#endif
