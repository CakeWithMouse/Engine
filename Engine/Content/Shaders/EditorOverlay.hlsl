cbuffer OverlayConstants : register(b0)
{
    float4x4 ViewProjection;
    float4 ScreenSize; // x: width, y: height, z: 1/width, w: 1/height
};

struct LineVSInput
{
    float3 Pos0 : POSITION0;
    float3 Pos1 : POSITION1;
    float4 Color : COLOR;
    float2 SideDir : TEXCOORD0; // x: -1 or +1, y: 0.0 (start) or 1.0 (end)
    float Thickness : TEXCOORD1; // width in pixels
};

struct LinePSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float DistFromCenter : TEXCOORD0;
    float HalfWidth : TEXCOORD1;
};

LinePSInput VSLine(LineVSInput input)
{
    LinePSInput output;
    output.Color = input.Color;

    if (ScreenSize.x <= 1.0f || ScreenSize.y <= 1.0f)
    {
        output.Position = float4(0, 0, -2.0f, 1.0f);
        output.DistFromCenter = 0.0f;
        output.HalfWidth = 1.0f;
        return output;
    }

    float4 c0 = mul(float4(input.Pos0, 1.0f), ViewProjection);
    float4 c1 = mul(float4(input.Pos1, 1.0f), ViewProjection);

    const float nearW = 0.1f;
    if (c0.w <= nearW && c1.w <= nearW)
    {
        output.Position = float4(0, 0, -2.0f, 1.0f);
        output.DistFromCenter = 0.0f;
        output.HalfWidth = 1.0f;
        return output;
    }

    float4 safeC0 = c0;
    float4 safeC1 = c1;
    if (safeC0.w < nearW)
    {
        float t = (nearW - safeC0.w) / max(1e-5f, (safeC1.w - safeC0.w));
        safeC0 = lerp(safeC0, safeC1, t);
    }
    else if (safeC1.w < nearW)
    {
        float t = (nearW - safeC1.w) / max(1e-5f, (safeC0.w - safeC1.w));
        safeC1 = lerp(safeC1, safeC0, t);
    }

    float2 ndc0 = safeC0.xy / safeC0.w;
    float2 ndc1 = safeC1.xy / safeC1.w;

    float2 screenDir = (ndc1 - ndc0) * float2(ScreenSize.x * 0.5f, ScreenSize.y * 0.5f);
    float len = length(screenDir);
    float2 dir = len > 1e-4f ? (screenDir / len) : float2(1.0f, 0.0f);
    float2 perp = float2(-dir.y, dir.x);

    // Expand by 0.5px margin on each side for smooth alpha falloff
    float halfWidth = input.Thickness * 0.5f + 0.5f;
    float2 pixelOffset = perp * (input.SideDir.x * halfWidth);
    float2 ndcOffset = pixelOffset * float2(ScreenSize.z * 2.0f, ScreenSize.w * 2.0f);

    float4 baseClip = lerp(safeC0, safeC1, input.SideDir.y);
    output.Position = float4(baseClip.xy + ndcOffset * baseClip.w, baseClip.zw);
    output.DistFromCenter = input.SideDir.x * halfWidth;
    output.HalfWidth = halfWidth;
    return output;
}

float4 PSLine(LinePSInput input) : SV_Target
{
    float d = abs(input.DistFromCenter);
    float alpha = 1.0f - smoothstep(max(0.0f, input.HalfWidth - 1.0f), input.HalfWidth, d);
    return float4(input.Color.rgb, input.Color.a * alpha);
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
    if (clipCenter.w <= 0.001f)
    {
        output.Position = float4(0.0f, 0.0f, -2.0f, 1.0f);
        output.UV = float2(0.0f, 0.0f);
        output.Tint = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    // Diameter is driven by the Editor via EngineOverlayBillboard::sizePixels
    // (fraction of physical viewport height, clamped in physical pixels).
    // input.Offset is the quad corner offset in physical screen pixels (= sizePixels/2).
    float2 pixelOffset = input.Offset;

    // Invert Y offset for screen coordinates (Y down in screen space, Y up in NDC)
    float2 ndcOffset = float2(pixelOffset.x * 2.0f * ScreenSize.z, -pixelOffset.y * 2.0f * ScreenSize.w) * clipCenter.w;
    output.Position = clipCenter + float4(ndcOffset.x, ndcOffset.y, 0.0f, 0.0f);
    output.UV = input.UV;
    output.Tint = input.Tint;
    return output;
}

Texture2D IconTexture : register(t0);
SamplerState IconSampler : register(s0);

float4 PSBillboard(BillboardPSInput input) : SV_Target
{
    float2 centered = input.UV - float2(0.5f, 0.5f);
    float dist = length(centered);
    if (dist > 0.5f)
    {
        discard;
    }

    float4 texColor = IconTexture.Sample(IconSampler, input.UV);
    
    // Contrast circular plate with antialiased edge
    float edgeAlpha = 1.0f - smoothstep(0.47f, 0.50f, dist);
    
    // Background badge plate: tinted light body with dark outer ring
    float3 plateColor = input.Tint.rgb;
    if (dist > 0.44f)
    {
        // Dark contrast rim for visibility against any background
        plateColor = float3(0.12f, 0.12f, 0.14f);
    }
    
    // If texture has glyph content (alpha > 0.1), blend it over the plate
    float3 finalRgb = plateColor;
    if (texColor.a > 0.1f)
    {
        finalRgb = lerp(plateColor, texColor.rgb, texColor.a);
    }
    else
    {
        // Procedural inner bulb / point symbol if texture alpha is zero
        if (dist < 0.22f)
        {
            finalRgb = float3(1.0f, 1.0f, 1.0f);
        }
    }

    return float4(finalRgb, edgeAlpha * input.Tint.a);
}

struct TriangleVSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct TrianglePSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

TrianglePSInput VSTriangle(TriangleVSInput input)
{
    TrianglePSInput output;
    output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
    output.Color = input.Color;
    return output;
}

float4 PSTriangle(TrianglePSInput input) : SV_Target
{
    return input.Color;
}
