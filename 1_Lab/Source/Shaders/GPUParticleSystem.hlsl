struct ParticleData
{
    float4 PositionLife;
    float4 VelocityLifetime;
    float4 ColorSize;
};

struct ParticleSortPair
{
    uint DepthKey;
    uint ParticleIndex;
};

cbuffer ParticleSimulationBuffer : register(b0)
{
    float DeltaTime;
    float TotalTime;
    uint ParticleCount;
    float BaseLifetime;
    float3 EmitterPosition;
    float SpreadRadius;
    float BaseSpeed;
    float SpeedRandomness;
    float MinLifeFraction;
    float MaxDistance;
    float3 BaseVelocity;
    float padding0;
    float3 Acceleration;
    float padding1;
    float4 BaseColor;
    float BaseSize;
    float SizeRandomness;
    float padding2;
    float padding3;
    float4x4 SimulationViewMatrix;
    float4x4 SimulationProjectionMatrix;
    float4x4 SimulationInvViewMatrix;
    float4x4 SimulationInvProjectionMatrix;
    float4 SimulationCameraPosition;
    float4 DepthCollisionParams; // x enabled, y shell in view units, z bounce, w friction
};

cbuffer ParticleSortBuffer : register(b1)
{
    float4x4 SortViewMatrix;
    uint SortParticleCount;
    uint SortElementCount;
    uint BitonicLevel;
    uint BitonicLevelMask;
};

StructuredBuffer<ParticleData> ParticlesIn : register(t0);
StructuredBuffer<ParticleSortPair> SortedParticlePairs : register(t1);
Texture2D<float> SceneDepthTexture : register(t2);
RWStructuredBuffer<ParticleData> ParticlesOut : register(u0);
RWStructuredBuffer<ParticleSortPair> ParticleSortPairs : register(u1);

float Hash11(float n)
{
    return frac(sin(n) * 43758.5453123);
}

float3 Hash33(float n)
{
    float3 q = float3(
        Hash11(n * 0.1031 + 1.17),
        Hash11(n * 0.11369 + 7.31),
        Hash11(n * 0.13787 + 9.73)
    );
    return q * 2.0 - 1.0;
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lenSq = dot(value, value);
    if (lenSq <= 0.0000001)
    {
        return fallback;
    }

    return value * rsqrt(lenSq);
}

bool ProjectWorldToDepthUV(float3 worldPosition, out float2 uv, out float deviceDepth)
{
    float4 viewPos = mul(float4(worldPosition, 1.0), SimulationViewMatrix);
    float4 clipPos = mul(viewPos, SimulationProjectionMatrix);
    if (clipPos.w <= 0.0001)
    {
        uv = float2(0.0, 0.0);
        deviceDepth = 1.0;
        return false;
    }

    float3 ndc = clipPos.xyz / clipPos.w;
    uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    deviceDepth = ndc.z;
    return uv.x >= 0.0 && uv.x <= 1.0 &&
           uv.y >= 0.0 && uv.y <= 1.0 &&
           deviceDepth >= 0.0 && deviceDepth <= 1.0;
}

float3 ReconstructWorldPosition(float2 uv, float depth)
{
    float4 clipPos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 viewPos = mul(clipPos, SimulationInvProjectionMatrix);
    viewPos /= max(viewPos.w, 0.0001);
    float4 worldPos = mul(viewPos, SimulationInvViewMatrix);
    return worldPos.xyz / max(worldPos.w, 0.0001);
}

float WorldViewDepth(float3 worldPosition)
{
    return mul(float4(worldPosition, 1.0), SimulationViewMatrix).z;
}

float LoadSceneDepth(int2 pixel, uint depthWidth, uint depthHeight)
{
    int2 clampedPixel = clamp(pixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1));
    return SceneDepthTexture.Load(int3(clampedPixel, 0));
}

float3 EstimateSceneNormal(int2 pixel, uint depthWidth, uint depthHeight, float2 centerUV, float centerDepth)
{
    float2 texelSize = 1.0 / float2(max(depthWidth, 1), max(depthHeight, 1));
    int2 leftPixel = pixel + int2(-1, 0);
    int2 rightPixel = pixel + int2(1, 0);
    int2 upPixel = pixel + int2(0, -1);
    int2 downPixel = pixel + int2(0, 1);

    float leftDepth = LoadSceneDepth(leftPixel, depthWidth, depthHeight);
    float rightDepth = LoadSceneDepth(rightPixel, depthWidth, depthHeight);
    float upDepth = LoadSceneDepth(upPixel, depthWidth, depthHeight);
    float downDepth = LoadSceneDepth(downPixel, depthWidth, depthHeight);

    float3 centerWorld = ReconstructWorldPosition(centerUV, centerDepth);
    float3 leftWorld = leftDepth < 0.99999 ? ReconstructWorldPosition((float2(clamp(leftPixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1))) + 0.5) * texelSize, leftDepth) : centerWorld;
    float3 rightWorld = rightDepth < 0.99999 ? ReconstructWorldPosition((float2(clamp(rightPixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1))) + 0.5) * texelSize, rightDepth) : centerWorld;
    float3 upWorld = upDepth < 0.99999 ? ReconstructWorldPosition((float2(clamp(upPixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1))) + 0.5) * texelSize, upDepth) : centerWorld;
    float3 downWorld = downDepth < 0.99999 ? ReconstructWorldPosition((float2(clamp(downPixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1))) + 0.5) * texelSize, downDepth) : centerWorld;

    float3 dx = rightWorld - leftWorld;
    float3 dy = downWorld - upWorld;
    float3 fallbackNormal = SafeNormalize(SimulationCameraPosition.xyz - centerWorld, float3(0.0, 1.0, 0.0));
    float3 normal = SafeNormalize(cross(dy, dx), fallbackNormal);

    if (dot(normal, fallbackNormal) < 0.0)
    {
        normal = -normal;
    }

    return normal;
}

void ResolveDepthCollision(inout ParticleData p, float3 previousPosition)
{
    if (DepthCollisionParams.x < 0.5)
    {
        return;
    }

    uint depthWidth = 0;
    uint depthHeight = 0;
    SceneDepthTexture.GetDimensions(depthWidth, depthHeight);
    if (depthWidth == 0 || depthHeight == 0)
    {
        return;
    }

    float2 uv;
    float particleDepth;
    if (!ProjectWorldToDepthUV(p.PositionLife.xyz, uv, particleDepth))
    {
        return;
    }

    int2 pixel = int2(saturate(uv) * float2(depthWidth, depthHeight));
    pixel = clamp(pixel, int2(0, 0), int2((int)depthWidth - 1, (int)depthHeight - 1));

    float sceneDepth = SceneDepthTexture.Load(int3(pixel, 0));
    if (sceneDepth >= 0.99999)
    {
        return;
    }

    float collisionShell = max(DepthCollisionParams.y, max(p.ColorSize.w * 0.35, 0.02));
    if (particleDepth + 0.000001 < sceneDepth)
    {
        return;
    }

    float3 sceneWorld = ReconstructWorldPosition((float2(pixel) + 0.5) / float2(depthWidth, depthHeight), sceneDepth);
    float3 normal = EstimateSceneNormal(pixel, depthWidth, depthHeight, (float2(pixel) + 0.5) / float2(depthWidth, depthHeight), sceneDepth);

    float radius = max(p.ColorSize.w * 0.65, 0.02);
    float particleViewDepth = WorldViewDepth(p.PositionLife.xyz);
    float sceneViewDepth = WorldViewDepth(sceneWorld);
    if (particleViewDepth < sceneViewDepth - collisionShell - radius)
    {
        return;
    }

    p.PositionLife.xyz = sceneWorld + normal * radius;

    float3 velocity = p.VelocityLifetime.xyz;
    float normalVelocity = dot(velocity, normal);
    if (normalVelocity < 0.0)
    {
        float bounce = max(DepthCollisionParams.z, 0.0);
        float friction = saturate(DepthCollisionParams.w);
        float3 reflected = reflect(velocity, normal) * bounce;
        float reflectedNormalVelocity = max(dot(reflected, normal), 0.0);
        float3 tangentVelocity = reflected - normal * reflectedNormalVelocity;
        p.VelocityLifetime.xyz = normal * reflectedNormalVelocity + tangentVelocity * (1.0 - friction);
    }
    else
    {
        float3 stepDir = SafeNormalize(p.PositionLife.xyz - previousPosition, normal);
        p.VelocityLifetime.xyz -= stepDir * min(dot(p.VelocityLifetime.xyz, stepDir), 0.0);
    }
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= ParticleCount)
    {
        return;
    }

    ParticleData p = ParticlesIn[index];
    float remainingLife = p.PositionLife.w - DeltaTime;
    float distanceFromEmitter = length(p.PositionLife.xyz - EmitterPosition);
    bool exceededDistance = (MaxDistance > 0.0) && (distanceFromEmitter >= MaxDistance);

    if (remainingLife <= 0.0 || exceededDistance)
    {
        float seed = (float)index + TotalTime * 17.0;
        float3 randomDir = normalize(Hash33(seed));
        float radialJitter = Hash11(seed * 1.73);
        float verticalJitter = Hash11(seed * 2.31);
        float3 spawnOffset = randomDir * (radialJitter * SpreadRadius);
        spawnOffset.y = (verticalJitter - 0.5) * SpreadRadius * 0.4;

        float speedScale = lerp(1.0 - SpeedRandomness, 1.0 + SpeedRandomness, Hash11(seed * 3.11));
        float lifeScale = lerp(MinLifeFraction, 1.0, Hash11(seed * 5.79));
        float sizeScale = lerp(1.0 - SizeRandomness, 1.0 + SizeRandomness, Hash11(seed * 9.41));

        p.PositionLife.xyz = EmitterPosition + spawnOffset;
        p.PositionLife.w = max(0.05, BaseLifetime * lifeScale);
        p.VelocityLifetime.xyz = BaseVelocity + randomDir * (BaseSpeed * speedScale);
        p.VelocityLifetime.w = p.PositionLife.w;
        p.ColorSize.rgb = BaseColor.rgb * lerp(0.82, 1.25, Hash11(seed * 13.13));
        p.ColorSize.w = max(0.01, BaseSize * sizeScale);
    }
    else
    {
        float3 previousPosition = p.PositionLife.xyz;
        p.PositionLife.w = remainingLife;
        p.VelocityLifetime.xyz += Acceleration * DeltaTime;
        p.PositionLife.xyz += p.VelocityLifetime.xyz * DeltaTime;
        ResolveDepthCollision(p, previousPosition);
    }

    ParticlesOut[index] = p;
}

uint FloatToOrderedUint(float value)
{
    uint bits = asuint(value);
    return (bits & 0x80000000u) ? ~bits : (bits ^ 0x80000000u);
}

bool SortPairGreater(ParticleSortPair a, ParticleSortPair b)
{
    return (a.DepthKey > b.DepthKey) ||
           (a.DepthKey == b.DepthKey && a.ParticleIndex > b.ParticleIndex);
}

bool SortPairLess(ParticleSortPair a, ParticleSortPair b)
{
    return (a.DepthKey < b.DepthKey) ||
           (a.DepthKey == b.DepthKey && a.ParticleIndex < b.ParticleIndex);
}

[numthreads(256, 1, 1)]
void CSBuildSortKeys(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= SortElementCount)
    {
        return;
    }

    ParticleSortPair pair;
    if (index < SortParticleCount)
    {
        ParticleData p = ParticlesIn[index];
        float viewDepth = mul(float4(p.PositionLife.xyz, 1.0), SortViewMatrix).z;
        bool validParticle = p.PositionLife.w > 0.0 && viewDepth > 0.0;
        pair.DepthKey = validParticle ? (0xffffffffu - FloatToOrderedUint(viewDepth)) : 0xffffffffu;
        pair.ParticleIndex = index;
    }
    else
    {
        pair.DepthKey = 0xffffffffu;
        pair.ParticleIndex = 0xffffffffu;
    }

    ParticleSortPairs[index] = pair;
}

[numthreads(256, 1, 1)]
void CSBitonicSort(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= SortElementCount)
    {
        return;
    }

    uint partnerIndex = index ^ BitonicLevelMask;
    if (partnerIndex <= index || partnerIndex >= SortElementCount)
    {
        return;
    }

    ParticleSortPair a = ParticleSortPairs[index];
    ParticleSortPair b = ParticleSortPairs[partnerIndex];

    bool ascending = ((index & BitonicLevel) == 0u);
    bool shouldSwap = ascending ? SortPairGreater(a, b) : SortPairLess(a, b);
    if (shouldSwap)
    {
        ParticleSortPairs[index] = b;
        ParticleSortPairs[partnerIndex] = a;
    }
}

cbuffer ParticleRenderBuffer : register(b0)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4 CameraRight;
    float4 CameraUp;
    float4 GlobalTint;
    float Brightness;
    float3 RenderPadding;
};

StructuredBuffer<ParticleData> ParticleBuffer : register(t0);

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VS_OUT VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VS_OUT output = (VS_OUT)0;
    uint particleIndex = SortedParticlePairs[instanceId].ParticleIndex;
    ParticleData p = ParticleBuffer[particleIndex];

    float lifeNorm = saturate(p.PositionLife.w / max(p.VelocityLifetime.w, 0.0001));
    float particleSize = p.ColorSize.w * lerp(0.35, 1.0, lifeNorm);

    float2 corners[4] =
    {
        float2(-1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0)
    };

    float2 corner = corners[vertexId & 3];
    float3 billboardOffset = (CameraRight.xyz * corner.x + CameraUp.xyz * corner.y) * particleSize;
    float3 worldPosition = p.PositionLife.xyz + billboardOffset;

    float4 viewPos = mul(float4(worldPosition, 1.0), ViewMatrix);
    output.Position = mul(viewPos, ProjectionMatrix);

    float alpha = saturate(lifeNorm * GlobalTint.a);
    output.Color = float4(saturate(p.ColorSize.rgb * GlobalTint.rgb), alpha);
    output.UV = corner * 0.5 + 0.5;
    return output;
}

float4 PSMain(VS_OUT input) : SV_Target
{
    float2 centeredUV = input.UV * 2.0 - 1.0;
    float radialRaw = 1.0 - dot(centeredUV, centeredUV);
    clip(radialRaw);
    float radial = saturate(radialRaw);

    float softAlpha = smoothstep(0.0, 0.12, radial);
    float glow = smoothstep(0.0, 1.0, radial);
    float3 color = input.Color.rgb * (0.9 + 0.35 * glow)     * Brightness;
    return float4(color, input.Color.a * softAlpha);
}
