#ifndef ENGINE_MATERIAL_COMMON_HLSLI
#define ENGINE_MATERIAL_COMMON_HLSLI

#include "../Common.hlsli"

// Contract between a material surface file and MaterialTemplate.hlsl.
//
// A surface file defines one function:
//     void Surface(MaterialInput i, inout MaterialOutput o)
// `o` arrives filled with defaults (component colour times the component's texture, interpolated
// normal), so a surface only overrides what it needs. Lighting, shadows and the forward/deferred variants come from the template.
//
// Resources of a surface:
//     cbuffer MaterialParams : register(b1) { ... };   parameters, listed in the editor by reflection
//     Texture2D ... : register(t6) and up;             t0-t5 belong to the engine
//     MaterialSampler (s2): linear filtering, wrap addressing
//
// Options a surface file may #define before its Surface function:
//     MATERIAL_EMISSIVE  the surface writes o.Emissive. The G-buffer has no emissive channel, so such
//                        a material is always drawn by the forward pass (also in deferred mode).
//     MATERIAL_UNLIT     same as "lighting": "unlit" in the .mat file.
//
// Set by the engine from the .mat file:
//     MATERIAL_BLEND       one of the MATERIAL_BLEND_* values below
//     MATERIAL_ALPHA_CLIP  alpha threshold of a masked material
//     MATERIAL_UNLIT       albedo + emissive are output as is, lights and shadows are ignored
//     MATERIAL_TWO_SIDED   back faces get the flipped normal
// Pass variants (shadow map and others) are listed in MaterialTemplate.hlsl; a surface does not
// need to know about them.

#define MATERIAL_BLEND_OPAQUE      0
#define MATERIAL_BLEND_MASKED      1 // opaque, pixels with alpha below the threshold are discarded
#define MATERIAL_BLEND_TRANSLUCENT 2 // alpha blended, sorted back to front, no depth write
#define MATERIAL_BLEND_ADDITIVE    3 // colour * alpha added to the scene

#ifndef MATERIAL_BLEND
#define MATERIAL_BLEND MATERIAL_BLEND_OPAQUE
#endif
#ifndef MATERIAL_ALPHA_CLIP
#define MATERIAL_ALPHA_CLIP 0.5f
#endif

SamplerState MaterialSampler : register(s2);

struct MaterialInput
{
    float3 WorldPos;
    float3 Normal;    // world space, normalized; flipped on back faces of two-sided materials
    // xyz world tangent along +U, w the sign of the bitangent. w is 0 when the mesh carries no
    // tangent (a skinned vertex, for instance) and NormalFromMap then falls back to derivatives.
    float4 Tangent;
    float2 UV;
    float3 ViewDir;   // from the surface towards the camera, normalized
    float4 Color;     // colour of the component (SetColor), times the colour of the copy when instanced
    float3 InstanceData; // per-copy custom data (MeshInstance::CustomData); 0 without one and for animated crowds
    float Time;       // real seconds since start
    bool FrontFace;
};

struct MaterialOutput
{
    float3 Albedo;         // base colour
    float3 Normal;         // world space; normalized by the template after Surface
    float SpecularWeight;  // 0 matte .. 1 strong highlight (its size is a scene-wide setting)
    float AmbientModel;    // 0 flat ambient, 1 hemispherical (brighter from above)
    float3 Emissive;       // added after lighting; requires #define MATERIAL_EMISSIVE
    float Alpha;           // used by masked, translucent and additive materials
};

// World normal from a tangent-space normal map sample (rgb in 0..1, DirectX convention: green
// points to +V, which is down the image). Pixel shader only.
//     o.Normal = NormalFromMap(i, NormalMap.Sample(MaterialSampler, i.UV).xyz, 1.0f);
//
// The basis comes from the mesh when it has one. Screen-space derivatives are the fallback, and
// they are only an approximation: they read the same frame for a whole 2x2 block of pixels, they
// break down along a uv seam, and they cannot tell a mirrored island from a normal one, so a
// mirrored half of a model lights up inside out. Where the vertices carry a tangent, the sign in
// w says which way the bitangent goes and all three problems disappear.
float3 NormalFromMap(MaterialInput i, float3 encoded, float strength)
{
    float3 n = encoded * 2.0f - 1.0f;
    n.xy *= strength;

    // w is interpolated, and a triangle can mix a vertex with a basis and one without (the pole of
    // a sphere), so only its sign is meaningful. The length check catches tangents that cancelled.
    if (abs(i.Tangent.w) > 0.5f && dot(i.Tangent.xyz, i.Tangent.xyz) > 1e-8f)
    {
        const float3 tangent = normalize(i.Tangent.xyz);
        const float3 bitangent = cross(i.Normal, tangent) * (i.Tangent.w < 0.0f ? -1.0f : 1.0f);
        return normalize(n.x * tangent + n.y * bitangent + n.z * i.Normal);
    }

    const float3 dp1 = ddx(i.WorldPos), dp2 = ddy(i.WorldPos);
    const float2 duv1 = ddx(i.UV), duv2 = ddy(i.UV);
    const float3 dp2perp = cross(dp2, i.Normal), dp1perp = cross(i.Normal, dp1);
    const float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    const float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    const float scale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-20f));
    return normalize(n.x * tangent * scale + n.y * bitangent * scale + n.z * i.Normal);
}

#endif
