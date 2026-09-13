cbuffer ConstantBuffer: register (b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 invViewMatrix;
    float4x4 invProjectionMatrix;
    float4 ObjectColor;
    float2 UVOffset;
    float HasTexture;
    float padding;
    float4 CameraPosition;
    float4 LightPositions[8];
    float4 LightColors[8];
    float4 LightParams[8];
    float4 LightMeta;
    float4 ReflectionData;
    float4x4 LightViewProjection[3];
    float4 CascadeSplits;
    float4 ShadowParams;
    float4 LightDirection;
    float4 DirectionalLightColorIntensity;
};

struct VS_IN
{
    float4 pos : POSITION;
    float4 col : COLOR;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 sampleDir : TEXCOORD0;
};

TextureCube envMap : register(t1);
SamplerState envSampler : register(s1);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;

    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.pos = mul(viewPos, projectionMatrix);
    output.sampleDir = normalize(worldPos.xyz - CameraPosition.xyz);
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 color = envMap.Sample(envSampler, input.sampleDir).rgb;
    float intensity = (ReflectionData.y > 0.001f) ? ReflectionData.y : 1.0f;
    color *= intensity;
    return float4(color, 1.0f);
}
