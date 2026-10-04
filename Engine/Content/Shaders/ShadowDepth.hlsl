cbuffer ShadowBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 lightViewProjection;
};

// Same instance stream as the colour pass (InstData in ShaderConstants.h).
struct InstData
{
    float4x4 World;
    float4 color;
};

StructuredBuffer<InstData> InstBuf : register(t0);

struct VS_IN
{
    float4 pos : POSITION;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    float4 worldPos = mul(input.pos, worldMatrix);
    output.pos = mul(worldPos, lightViewProjection);
    return output;
}

VS_OUT VSMainInstanced(VS_IN input, uint instanceId : SV_InstanceID)
{
    VS_OUT output = (VS_OUT)0;
    float4 worldPos = mul(input.pos, InstBuf[instanceId].World);
    output.pos = mul(worldPos, lightViewProjection);
    return output;
}
