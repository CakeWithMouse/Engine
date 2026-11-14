cbuffer MaterialParams : register(b1)
{
    float4 Tint = float4(1.0,0.05,0.05, 1.0f);
    float Opacity = 0.5f;
};
void Surface(MaterialInput i, inout MaterialOutput o)
{
    o.Albedo = Tint.rgb;
    o.Alpha = Opacity;
    o.AmbientModel = 0.0f;
}
