#include "CalmLodRow.h"
#include <Engine/Render/MeshPrimitives.h>

CalmLodRow::CalmLodRow()
{
    SetLod(0, MeshPrimitives::Sphere(0.35f, 24, 12));
    SetAutoLod({0.5f});
    SetLodScreenHeights({18.0f});
    SetDrawDistance(90.0f);
    SetLodDebugColors(true);
    for (int i = 0; i < 8; ++i) {
        MeshInstance lamp;
        lamp.Position = {-7.0f + 2.0f * i, 0.35f, 2.8f};
        lamp.Scale = 1.4f;
        lamp.Color = (i % 2) ? glm::vec4(0.92f, 0.68f, 0.34f, 1.0f)
                             : glm::vec4(0.72f, 0.78f, 0.83f, 1.0f);
        AddInstance(lamp);
    }
}
