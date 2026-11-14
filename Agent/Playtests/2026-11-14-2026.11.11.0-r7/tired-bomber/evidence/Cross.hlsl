void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo=i.UV.x>1.5 ? float3(0,0,1) : float3(1,0,0); o.Alpha=i.Color.a; }
