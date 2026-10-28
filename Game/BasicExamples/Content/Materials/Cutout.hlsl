// Masked: a grid of round holes; two-sided, so the inner walls show through. The holes of opposite
// faces line up, so light passes through the cube and the shadow is perforated too.
cbuffer MaterialParams : register(b1)
{
    float4 Tint = float4(0.30f, 0.85f, 0.40f, 1.0f);
    float Holes = 4.0f;       // per face side
    float HoleRadius = 0.32f; // fraction of a cell
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float2 cell = frac(i.UV * Holes) - 0.5f;
    o.Albedo = i.FrontFace ? Tint.rgb : Tint.rgb * 0.45f;
    o.Alpha = length(cell) > HoleRadius ? 1.0f : 0.0f;
}
