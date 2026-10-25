// Translucent: thin in the middle, more opaque at the edges.
cbuffer MaterialParams : register(b1)
{
    float4 Tint = float4(0.55f, 0.80f, 1.0f, 1.0f);
    float Opacity = 0.25f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float fresnel = pow(1.0f - saturate(dot(i.Normal, i.ViewDir)), 2.0f);
    o.Albedo = Tint.rgb;
    o.Alpha = lerp(Opacity, 0.9f, fresnel);
    o.SpecularWeight = 1.0f;
}
