// Basic imported material. Edit Tint and BaseMap in the material editor.
cbuffer MaterialParams : register(b1) { float4 Tint = float4(1, 1, 1, 1); };
Texture2D BaseMap : register(t6);
void Surface(MaterialInput i, inout MaterialOutput o)
{
    float4 color = BaseMap.Sample(MaterialSampler, i.UV) * Tint;
    o.Albedo *= color.rgb;
    o.Alpha *= color.a;
}
