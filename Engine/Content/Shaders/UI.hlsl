// Screen-space interface: panels, bars and text in one pass, after the scene is finished.
// The vertex carries pixels in position.xy and the atlas coordinate in position.zw, so the UI
// reuses the engine's Primitive input layout instead of asking for one of its own.

cbuffer UIBuffer : register(b0)
{
    float4 ScreenSize; // x width, y height, zw unused
};

Texture2D uiAtlas : register(t0);
SamplerState uiSampler : register(s0);

struct VS_IN
{
    float4 pos : POSITION;
    float4 color : COLOR;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    // Pixels, origin at the top left, into clip space.
    const float2 normalized = input.pos.xy / max(ScreenSize.xy, float2(1.0f, 1.0f));
    output.pos = float4(normalized.x * 2.0f - 1.0f, 1.0f - normalized.y * 2.0f, 0.0f, 1.0f);
    output.uv = input.pos.zw;
    output.color = input.color;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    // The atlas is white where a glyph covers the pixel and white in the solid block, so a panel
    // and a letter take the same path: the colour is the element's, the coverage is the atlas's.
    const float coverage = uiAtlas.Sample(uiSampler, input.uv).r;
    return float4(input.color.rgb, input.color.a * coverage);
}
