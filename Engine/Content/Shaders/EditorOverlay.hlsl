cbuffer OverlayConstants : register(b0)
{
    float4x4 ViewProjection;
    float4 ScreenSize; // x: width, y: height, z: 1/width, w: 1/height
};

struct LineVSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct LinePSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

LinePSInput VSLine(LineVSInput input)
{
    LinePSInput output;
    output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
    output.Color = input.Color;
    return output;
}

float4 PSLine(LinePSInput input) : SV_Target
{
    return input.Color;
}

struct BillboardVSInput
{
    float3 Position : POSITION;
    float2 Offset : TEXCOORD0; // offset in screen pixels
    float2 UV : TEXCOORD1;
    float4 Tint : COLOR;
};

struct BillboardPSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Tint : COLOR;
};

BillboardPSInput VSBillboard(BillboardVSInput input)
{
    BillboardPSInput output;
    float4 clipCenter = mul(float4(input.Position, 1.0f), ViewProjection);
    // Invert Y offset for screen coordinates (Y down in screen space, Y up in NDC)
    float2 ndcOffset = float2(input.Offset.x * 2.0f * ScreenSize.z, -input.Offset.y * 2.0f * ScreenSize.w) * clipCenter.w;
    output.Position = clipCenter + float4(ndcOffset.x, ndcOffset.y, 0.0f, 0.0f);
    output.UV = input.UV;
    output.Tint = input.Tint;
    return output;
}

Texture2D IconTexture : register(t0);
SamplerState IconSampler : register(s0);

float4 PSBillboard(BillboardPSInput input) : SV_Target
{
    float4 texColor = IconTexture.Sample(IconSampler, input.UV);
    return texColor * input.Tint;
}

