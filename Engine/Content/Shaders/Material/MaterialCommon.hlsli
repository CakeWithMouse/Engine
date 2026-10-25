#ifndef ENGINE_MATERIAL_COMMON_HLSLI
#define ENGINE_MATERIAL_COMMON_HLSLI

#include "../Common.hlsli"

// Contract between a material surface file and MaterialTemplate.hlsl.
//
// A surface file defines one function:
//     void Surface(MaterialInput i, inout MaterialOutput o)
// `o` arrives filled with defaults (component colour, interpolated normal), so a surface only
// overrides what it needs. Lighting, shadows and the forward/deferred variants come from the template.
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
    float2 UV;
    float3 ViewDir;   // from the surface towards the camera, normalized
    float4 Color;     // colour of the component (SetColor)
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

#endif
