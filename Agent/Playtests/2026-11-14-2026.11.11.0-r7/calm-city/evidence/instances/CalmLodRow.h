#pragma once

#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>

ENGINE_COMPONENT("Street lamp LOD row", "CalmCity.LodRow")
class CalmLodRow final : public InstancedMeshComponent
{
public:
    CalmLodRow();
};
