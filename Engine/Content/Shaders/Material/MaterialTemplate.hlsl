#include "MaterialCommon.hlsli"
// The engine maps this include to the surface file of the material being compiled.
#include "MaterialSurface.hlsli"

// Shader of every material asset: VS for VertexFormat::Mesh plus forward and G-buffer pixel
// shaders around the Surface function. The G-buffer variant exists only for lit opaque/masked
// materials without emissive; for the others DEFERRED_GBUFFER compiles the forward shader
// (one render target), and the renderer draws them in its forward-only pass.

#if !defined(MATERIAL_UNLIT) && !defined(MATERIAL_EMISSIVE) && MATERIAL_BLEND <= MATERIAL_BLEND_MASKED
#define MATERIAL_DEFERRED 1
#endif

static const float MaterialDefaultSpecularWeight = 0.25f;
static const float MaterialDefaultAmbientModel = 1.0f;

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
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    float4 worldPos = mul(input.pos, worldMatrix);
    output.pos = mul(mul(worldPos, viewMatrix), projectionMatrix);
    output.worldPos = worldPos.xyz;
    // Inverse-transpose keeps normals perpendicular under non-uniform scale.
    output.normal = mul(input.normal, (float3x3)normalMatrix);
    output.uv = input.uv;
    return output;
}

MaterialOutput EvaluateMaterial(PS_IN input, bool frontFace)
{
    float3 normal = normalize(input.normal);
#if defined(MATERIAL_TWO_SIDED)
    normal = frontFace ? normal : -normal;
#endif

    MaterialInput i;
    i.WorldPos = input.worldPos;
    i.Normal = normal;
    i.UV = input.uv;
    i.ViewDir = normalize(CameraPosition.xyz - input.worldPos);
    i.Color = ObjectColor;
    i.Time = TimeParams.x;
    i.FrontFace = frontFace;

    MaterialOutput o;
    o.Albedo = ObjectColor.rgb;
    o.Normal = normal;
    o.SpecularWeight = MaterialDefaultSpecularWeight;
    o.AmbientModel = MaterialDefaultAmbientModel;
    o.Emissive = float3(0.0f, 0.0f, 0.0f);
    o.Alpha = ObjectColor.a;

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

#if defined(DEFERRED_GBUFFER) && defined(MATERIAL_DEFERRED)
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
