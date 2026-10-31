// Mass units and scenery. Base colour is the colour of the copy (army colour of a unit), lit like the
// old InstancedMesh shader. Each copy can be muddy near the ground: InstanceData.x is its wear
// (0 clean .. 1 dirty), set per unit by the game. Far units are sprites baked without wear.
cbuffer MaterialParams : register(b1)
{
    float Gloss = 0.35f;
    float4 MudColor = float4(0.24f, 0.19f, 0.13f, 1.0f);
    float MudHeight = 1.2f;  // world units above the ground the mud reaches
    float MudAmount = 0.8f;  // how dirty a unit with wear 1 gets
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float lowness = saturate(1.0f - i.WorldPos.y / max(MudHeight, 0.001f));
    float mud = saturate(i.InstanceData.x * MudAmount) * lowness;
    o.Albedo = lerp(i.Color.rgb, MudColor.rgb, mud);
    o.SpecularWeight = Gloss * (1.0f - mud);
}
