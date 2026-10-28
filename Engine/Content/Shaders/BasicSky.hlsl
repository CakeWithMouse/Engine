#include "Common.hlsli"
struct Vertex { float4 pos : POSITION; float4 color : COLOR; };
struct Pixel { float4 pos : SV_POSITION; float3 direction : TEXCOORD0; };
Pixel VSMain(Vertex v)
{
    Pixel p;
    p.direction = mul(float4(v.pos.xyz, 0), worldMatrix).xyz;
    p.pos = mul(mul(float4(CameraPosition.xyz + p.direction, 1), viewMatrix), projectionMatrix);
    p.pos.z = p.pos.w;
    return p;
}
float4 PSMain(Pixel p) : SV_Target
{
    float height = normalize(p.direction).y;
    float3 horizon = float3(0.66, 0.79, 0.92);
    float3 color = height >= 0 ? lerp(horizon, float3(0.12,0.34,0.72), pow(saturate(height),0.6))
                              : lerp(horizon, float3(0.16,0.19,0.25), saturate(-height*3));
    return float4(color,1);
}
