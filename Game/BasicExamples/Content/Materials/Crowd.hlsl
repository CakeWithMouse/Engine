// Instanced crowd. The hue is the colour of each copy (it also tints the sprites of the far level);
// InstanceData.x is the flicker phase of the copy (sprites keep the baked, unflickering look).
cbuffer MaterialParams : register(b1)
{
    float FlickerSpeed = 1.5f;
    float FlickerAmount = 0.35f;
};

void Surface(MaterialInput i, inout MaterialOutput o)
{
    float wave = 0.5f + 0.5f * sin(i.Time * FlickerSpeed + i.InstanceData.x * 6.2831853f);
    o.Albedo = i.Color.rgb * (1.0f - FlickerAmount * wave);
    o.SpecularWeight = 0.4f;
}
