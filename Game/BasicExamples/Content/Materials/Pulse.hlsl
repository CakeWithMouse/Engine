// Emissive glow animated with time. Emissive materials are drawn forward in both render modes.
#define MATERIAL_EMISSIVE

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor = float4(0.20f, 0.03f, 0.02f, 1.0f);
    float4 GlowColor = float4(1.0f, 0.30f, 0.05f, 1.0f);
    float Speed = 2.0f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float pulse = sin(i.Time * Speed) * 0.5f + 0.5f;
    o.Albedo = BaseColor.rgb;
    o.Emissive = GlowColor.rgb * pulse;
}
