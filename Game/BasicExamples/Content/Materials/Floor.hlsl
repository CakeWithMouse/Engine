// Lit checker from UVs: two colours and the number of tiles are parameters.
cbuffer MaterialParams : register(b1)
{
    float4 ColorA = float4(0.80f, 0.80f, 0.84f, 1.0f);
    float4 ColorB = float4(0.30f, 0.32f, 0.38f, 1.0f);
    float Tiles = 10.0f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float2 cell = floor(i.UV * Tiles);
    float checker = fmod(cell.x + cell.y, 2.0f);
    o.Albedo = lerp(ColorA.rgb, ColorB.rgb, checker);
    o.SpecularWeight = 0.08f;
}
