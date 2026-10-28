cbuffer Selection : register(b0) { float4 corners[24]; };
float4 VSMain(uint id : SV_VertexID) : SV_POSITION { return corners[id]; }
float4 PSMain() : SV_Target { return float4(1.0,0.65,0.04,1); }
