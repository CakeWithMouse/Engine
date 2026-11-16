#define MATERIAL_UNLIT
void Surface(MaterialInput i, inout MaterialOutput o)
{
    o.Albedo = float3(0.05f, 0.8f, 0.18f);
    o.Alpha = 1.0f;
}
