// Drawn instead of a material whose shader does not compile: an unlit magenta/black world-space
// checker that cannot be mistaken for real content.
#define MATERIAL_UNLIT

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float3 cell = floor(i.WorldPos * 2.0f);
    float checker = fmod(abs(cell.x + cell.y + cell.z), 2.0f);
    o.Albedo = checker < 0.5f ? float3(1.0f, 0.0f, 1.0f) : float3(0.08f, 0.0f, 0.08f);
    o.Alpha = 1.0f;
}
