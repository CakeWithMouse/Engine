#ifndef ENGINE_SHADOW_DEPTH_HLSLI
#define ENGINE_SHADOW_DEPTH_HLSLI

// Kept in a small shared include so both forward/deferred lighting and the legacy sphere shader
// use identical camera-depth and cascade-selection semantics.
float ShadowViewDepth(float3 worldPos, float4x4 viewMatrix)
{
    // Cascade splits are measured along the camera forward axis, matching the CPU frustum fitter.
    return max(mul(float4(worldPos, 1.0f), viewMatrix).z, 0.0f);
}

int SelectShadowCascade(float viewDepth, float4 cascadeSplits)
{
    if (viewDepth < cascadeSplits.x) return 0;
    if (viewDepth < cascadeSplits.y) return 1;
    return 2;
}

#endif
