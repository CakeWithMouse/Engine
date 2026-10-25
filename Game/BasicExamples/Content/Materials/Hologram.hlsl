// Additive and unlit: glowing edges and scan lines moving up.
cbuffer MaterialParams : register(b1)
{
    float4 Tint = float4(0.10f, 0.90f, 1.0f, 1.0f);
    float LineDensity = 6.0f;
    float Speed = 0.6f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float edge = pow(1.0f - saturate(abs(dot(i.Normal, i.ViewDir))), 2.0f);
    float scan = pow(frac(i.WorldPos.y * LineDensity - i.Time * Speed), 6.0f);
    o.Albedo = Tint.rgb;
    o.Alpha = saturate(0.08f + edge * 0.9f + scan * 0.5f);
}
