Texture2D MaskMap : register(t6);
void Surface(MaterialInput i, inout MaterialOutput o) { float4 m=MaskMap.Sample(MaterialSampler,i.UV); o.Albedo=float3(0.12,0.82,0.18); o.Alpha=m.a; }
