cbuffer MaterialParams : register(b1) { float T32 = 0.0f; float T127 = 0.0f; };
void Surface(MaterialInput i, inout MaterialOutput o)
{
    o.Albedo = float3(T32 > 0.0f ? 1.0f : 0.0f, T127 > 0.0f ? 1.0f : 0.0f, 0.25f);
}
