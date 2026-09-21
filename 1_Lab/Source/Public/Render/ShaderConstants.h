#pragma once
#include <directxmath.h>

constexpr int MaxPointLights = 8;
constexpr int MaxShadowCascades = 3;

/**
 * Layout of `cbuffer ConstantBuffer : register(b0)` from Shaders/Common.hlsli.
 * Matrices are stored transposed (HLSL column_major packing + mul(v, M)).
 * Fields may only be appended: legacy shaders declare a prefix of this layout.
 */
struct ConstantBufferData
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 ObjectColor;
    DirectX::XMFLOAT2 UVOffset;
    float HasTexture = 0.0f;
    float padding = 0.0f;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[MaxPointLights];
    DirectX::XMFLOAT4 LightColors[MaxPointLights];
    DirectX::XMFLOAT4 LightParams[MaxPointLights];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4 ReflectionData;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
    // Inverse-transpose of the world matrix for normals (stored in the same HLSL layout as worldMatrix).
    DirectX::XMFLOAT4X4 normalMatrix;
};
static_assert(sizeof(ConstantBufferData) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes");

// The deferred lighting pass reads the same cbuffer as the materials.
using DeferredLightingBufferData = ConstantBufferData;

/** Layout of the cbuffer in Shaders/InstanceSphereFigure.hlsl. */
struct InstConstantBufferData
{
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[MaxPointLights];
    DirectX::XMFLOAT4 LightColors[MaxPointLights];
    DirectX::XMFLOAT4 LightParams[MaxPointLights];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4 ReflectionData;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
};
static_assert(sizeof(InstConstantBufferData) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes");

/** One element of the instance stream (StructuredBuffer<InstData> at t0). */
struct InstData
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4 ObjectColor;
};

/** Layout of the cbuffer in Shaders/ShadowDepth.hlsl. */
struct ShadowPassBufferData
{
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 lightViewProjection;
};

/**
 * Per-frame snapshot shared by every pass. Built once per frame after the camera,
 * world transforms and shadow cascades are final; objects only copy it.
 */
struct FrameConstants
{
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    DirectX::XMFLOAT4X4 invViewMatrix;
    DirectX::XMFLOAT4X4 invProjectionMatrix;
    DirectX::XMFLOAT4 CameraPosition;
    DirectX::XMFLOAT4 LightPositions[MaxPointLights];
    DirectX::XMFLOAT4 LightColors[MaxPointLights];
    DirectX::XMFLOAT4 LightParams[MaxPointLights];
    DirectX::XMFLOAT4 LightMeta;
    DirectX::XMFLOAT4X4 LightViewProjection[MaxShadowCascades];
    DirectX::XMFLOAT4 CascadeSplits;
    DirectX::XMFLOAT4 ShadowParams;
    DirectX::XMFLOAT4 LightDirection;
    DirectX::XMFLOAT4 DirectionalLightColorIntensity;
    // Camera frustum planes (xyz = inward normal, w = distance) for sphere culling.
    DirectX::XMFLOAT4 FrustumPlanes[6];
};

template <typename TBuffer>
void ApplyFrameConstants(TBuffer& target, const FrameConstants& frame)
{
    target.viewMatrix = frame.viewMatrix;
    target.projectionMatrix = frame.projectionMatrix;
    target.invViewMatrix = frame.invViewMatrix;
    target.invProjectionMatrix = frame.invProjectionMatrix;
    target.CameraPosition = frame.CameraPosition;
    for (int i = 0; i < MaxPointLights; ++i)
    {
        target.LightPositions[i] = frame.LightPositions[i];
        target.LightColors[i] = frame.LightColors[i];
        target.LightParams[i] = frame.LightParams[i];
    }
    target.LightMeta = frame.LightMeta;
    for (int i = 0; i < MaxShadowCascades; ++i)
    {
        target.LightViewProjection[i] = frame.LightViewProjection[i];
    }
    target.CascadeSplits = frame.CascadeSplits;
    target.ShadowParams = frame.ShadowParams;
    target.LightDirection = frame.LightDirection;
    target.DirectionalLightColorIntensity = frame.DirectionalLightColorIntensity;
}
