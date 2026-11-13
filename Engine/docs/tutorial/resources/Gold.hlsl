// Lit metal look: strong highlight and a brighter rim at grazing angles.
cbuffer MaterialParams : register(b1)
{
    float4 Tint = float4(1.0f, 0.72f, 0.28f, 1.0f);
    float Gloss = 1.0f;
    float Rim = 0.45f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float fresnel = pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), 3.0f);
    o.Albedo = saturate(Tint.rgb + Rim * fresnel);
    o.SpecularWeight = Gloss;
    o.AmbientModel = 0.0f;
}
