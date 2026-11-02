cbuffer FxaaCB : register(b0)
{
    float2 InvScreenSize;
    float2 Padding;
};

Texture2D SceneTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    output.TexCoord = float2((id << 1) & 2, id & 2);
    output.Position = float4(output.TexCoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float FxaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 uv = input.TexCoord;
    float4 rgbM = SceneTexture.Sample(LinearClampSampler, uv);

    float lumaM = FxaaLuma(rgbM.rgb);
    float lumaN = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2(0.0, -InvScreenSize.y)).rgb);
    float lumaS = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2(0.0,  InvScreenSize.y)).rgb);
    float lumaW = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2(-InvScreenSize.x, 0.0)).rgb);
    float lumaE = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2( InvScreenSize.x, 0.0)).rgb);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float range = lumaMax - lumaMin;

    if (range < max(0.02, lumaMax * 0.08))
    {
        return rgbM;
    }

    float lumaNW = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2(-InvScreenSize.x, -InvScreenSize.y)).rgb);
    float lumaNE = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2( InvScreenSize.x, -InvScreenSize.y)).rgb);
    float lumaSW = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2(-InvScreenSize.x,  InvScreenSize.y)).rgb);
    float lumaSE = FxaaLuma(SceneTexture.Sample(LinearClampSampler, uv + float2( InvScreenSize.x,  InvScreenSize.y)).rgb);

    float edgeVert = abs((0.25 * lumaNW) + (-0.5 * lumaN) + (0.25 * lumaNE)) +
                     abs((0.50 * lumaW ) + (-1.0 * lumaM) + (0.50 * lumaE )) +
                     abs((0.25 * lumaSW) + (-0.5 * lumaS) + (0.25 * lumaSE));
    float edgeHorz = abs((0.25 * lumaNW) + (-0.5 * lumaW) + (0.25 * lumaSW)) +
                     abs((0.50 * lumaN ) + (-1.0 * lumaM) + (0.50 * lumaS )) +
                     abs((0.25 * lumaNE) + (-0.5 * lumaE) + (0.25 * lumaSE));

    bool isHorz = (edgeHorz >= edgeVert);

    float luma1 = isHorz ? lumaN : lumaW;
    float luma2 = isHorz ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);

    bool is1Steeper = (gradient1 >= gradient2);
    float stepSign = is1Steeper ? -1.0 : 1.0;

    float2 stepDir = isHorz ? float2(0.0, stepSign * InvScreenSize.y) : float2(stepSign * InvScreenSize.x, 0.0);

    float subpix = (2.0 * (lumaN + lumaS + lumaW + lumaE) + (lumaNW + lumaNE + lumaSW + lumaSE)) * (1.0 / 12.0);
    subpix = clamp(abs(subpix - lumaM) / range, 0.0, 1.0);
    float blendFactor = subpix * subpix * 0.85;

    float4 rgbBlend = SceneTexture.Sample(LinearClampSampler, uv + stepDir * (0.5 * blendFactor));
    float3 finalRgb = lerp(rgbM.rgb, rgbBlend.rgb, blendFactor);
    return float4(finalRgb, 1.0f);
}

