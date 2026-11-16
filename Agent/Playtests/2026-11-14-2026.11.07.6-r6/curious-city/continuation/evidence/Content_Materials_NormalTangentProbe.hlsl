Texture2D NormalMap : register(t6);
void Surface(MaterialInput i, inout MaterialOutput o)
{
    float3 mapped = NormalFromMap(i, NormalMap.Sample(MaterialSampler, i.UV).xyz, 1.0f);
    o.Albedo = mapped * 0.5f + 0.5f;
    o.Normal = mapped;
}
