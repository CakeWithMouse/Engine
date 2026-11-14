Texture2D MaskMap : register(t6); void Surface(MaterialInput i,inout MaterialOutput o){float4 m=MaskMap.Sample(MaterialSampler,i.UV);o.Albedo=float3(0.05,1,0.1);o.Alpha=m.a;}
