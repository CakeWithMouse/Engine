#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/PropertyMacros.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Ai
{
class World;

/**
 * Scene object a unit can use: a bench, a cannon, a door. Slots are claimed, so two units do
 * not take the same place, like Smart Objects in Unreal. Found by tag.
 */
class SmartObject : public GameComponent
{
public:
    std::string tag = "Seat";
    int slots = 1;

    ENGINE_PROPERTIES(GameComponent,
        ENGINE_PROPERTY_FIELD(tag, "Метка")
        ENGINE_PROPERTY_FIELD(slots, "Мест", 1, 64)
    )
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    void BeginPlay() override;

    int FreeSlots() const { return slots - static_cast<int>(users_.size()); }
    /** controller is Controller serial; false when every slot is taken. */
    bool Claim(uint32_t controller);
    void Release(uint32_t controller);
    bool IsUsedBy(uint32_t controller) const;

private:
    std::vector<uint32_t> users_;
};
} // namespace Ai
