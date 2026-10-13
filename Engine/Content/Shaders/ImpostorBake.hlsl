// Renders a model into one tile of the impostor atlas: colour with coverage in alpha and the
// object-space normal. Runs once when the model is loaded, never during a normal frame.
cbuffer ImpostorBakeBuffer : register(b0)
{
    float4x4 viewProjection; // orthographic, fitted to the model bounds
    float4 baseColor;
    float4 bakeFlags;        // x = model has a texture
};

Texture2D modelTexture : register(t0);
SamplerState modelSampler : register(s0);

struct VS_IN
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct BakeOut
{
    float4 Albedo : SV_Target0; // rgb colour, a = coverage
    float4 Normal : SV_Target1; // object-space normal packed to 0..1
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    output.pos = mul(input.pos, viewProjection);
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    return output;
}

BakeOut PSMain(PS_IN input)
{
    float4 color = bakeFlags.x > 0.5f ? modelTexture.Sample(modelSampler, input.texCoord) * baseColor : baseColor;

    BakeOut output;
    output.Albedo = float4(color.rgb, 1.0f);
    output.Normal = float4(normalize(input.normal) * 0.5f + 0.5f, 1.0f);
    return output;
}
