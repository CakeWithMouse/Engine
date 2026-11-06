#pragma once

#include <Engine/Render/ShaderConstants.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <directxmath.h>

/**
 * View and projection of one cube face, transposed for HLSL like the cascade matrices. Faces go in
 * Direct3D cube order (+X, -X, +Y, -Y, +Z, -Z), and the up vectors are the ones that make a face
 * rendered here line up with what TextureCube sampling reads for the same direction.
 */
inline DirectX::XMFLOAT4X4 PointShadowViewProjection(int face, const DirectX::XMFLOAT3& position, float farPlane)
{
    using namespace DirectX;
    static const XMFLOAT3 directions[PointShadowFaces] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}};
    static const XMFLOAT3 ups[PointShadowFaces] = {
        {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

    const XMMATRIX view = XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&directions[face]), XMLoadFloat3(&ups[face]));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, PointShadowNearPlane, farPlane);
    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, XMMatrixTranspose(XMMatrixMultiply(view, projection)));
    return result;
}

/** Depth a face stores for a point this far along its axis. Mirrors PointShadowDepth in Common.hlsli. */
inline float PointShadowDepth(float axisDistance, float farPlane)
{
    return farPlane / (farPlane - PointShadowNearPlane) * (1.0f - PointShadowNearPlane / axisDistance);
}

/**
 * Faces a sphere can touch, one bit per face, for a sphere at (x, y, z) from the light. A face is a
 * pyramid bounded by the diagonal planes through the light, so the test only widens those by the
 * radius. Conservative at the corners; a sphere around the light touches all six.
 */
inline uint32_t PointShadowFaceMask(float x, float y, float z, float radius)
{
    const float reach = radius * 1.41421356f;
    const float ax = std::abs(x);
    const float ay = std::abs(y);
    const float az = std::abs(z);
    uint32_t mask = 0;
    if (x + reach >= ay && x + reach >= az) mask |= 1u << 0;
    if (-x + reach >= ay && -x + reach >= az) mask |= 1u << 1;
    if (y + reach >= ax && y + reach >= az) mask |= 1u << 2;
    if (-y + reach >= ax && -y + reach >= az) mask |= 1u << 3;
    if (z + reach >= ax && z + reach >= ay) mask |= 1u << 4;
    if (-z + reach >= ax && -z + reach >= ay) mask |= 1u << 5;
    return mask;
}
