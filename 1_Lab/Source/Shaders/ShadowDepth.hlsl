cbuffer ShadowBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 lightViewProjection;
};

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

