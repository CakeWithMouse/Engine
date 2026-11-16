#include <Engine/AI/AiCharacter.h>
#include <Engine/AI/MachineFile.h>
#include <Engine/AI/SmartObject.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

void AiCharacter::BeginPlay()
{
    Character::BeginPlay();
    Ai::World& world = Ai::Get(*GetGame());
    if (playerControlled)
    {
        // The player is never driven by AI, but AI senses see it as a member of its team.
        world.AddTarget(*this, {static_cast<uint8_t>(std::clamp(team, 0, 255))});
        return;
    }
    // Players the game did not register (a plain Gameplay.Player in a project without C++) are seen as team 1.
    // The character's own scene: while a save game loads, the world may still point to the old one.
    if (Scene* scene = &GetGame()->GetScene())
        for (const auto& [id, component] : scene->GetComponentsById())
        {
            auto* character = dynamic_cast<Character*>(component);
            // While a save game loads, an object may be listed before it gets its id: it is skipped (the next unit adds it).
            if (character && character->playerControlled && character->GetComponentId() == id && !dynamic_cast<AiCharacter*>(character) &&
                !world.IsTarget(id))
                world.AddTarget(*character, {static_cast<uint8_t>(PlayerTeam)});
        }
    if (world.Find(GetComponentId())) return;
    Ai::Controller& ai = world.Spawn(*this, CreateController(world));
    Ai::Perception& senses = ai.GetPerception();
    senses.SetTeam({static_cast<uint8_t>(std::clamp(team, 0, 255))});
    if (sightRadius > 0) senses.EnableSight(sightRadius, fieldOfView);
    if (hearingRadius > 0) senses.EnableHearing(hearingRadius);
    SetupAi(ai);
    // A machine started by SetupAi wins; otherwise the one named in the property runs.
    if (!ai.IsRunning() && !stateMachine.empty())
    {
        try
        {
            if (Ai::IsMachineFile(stateMachine))
            {
                // A .fsm from the AI editor: every problem is listed, the first ones in the message.
                const Ai::MachineLoadResult loaded = Ai::LoadMachineFile(stateMachine);
                if (!loaded.asset)
                {
                    std::string message = stateMachine + ":";
                    for (size_t i = 0; i < loaded.problems.size() && i < 3; ++i)
                    {
                        const auto& problem = loaded.problems[i];
                        message += (i ? "; " : " ") + (problem.node ? "node " + std::to_string(problem.node) + ": " : std::string()) + problem.message;
                    }
                    if (loaded.problems.size() > 3) message += " (and " + std::to_string(loaded.problems.size() - 3) + " more)";
                    throw std::invalid_argument(message);
                }
                loaded.asset->Run(ai, machineMemory);
            }
            else ai.Run(Ai::GetMachine(stateMachine));
        }
        catch (const std::exception& error) { throw std::invalid_argument("AiCharacter (" + ai.Name() + "): " + error.what()); }
    }
}

Ai::Controller* AiCharacter::GetAi() const
{
    Game* game = GetGame();
    auto* subsystem = game ? game->GetSubsystem<Ai::Subsystem>() : nullptr;
    return subsystem ? subsystem->GetWorld().Find(GetComponentId()) : nullptr;
}

std::unique_ptr<Ai::Controller> AiCharacter::CreateController(Ai::World& world) { return std::make_unique<Ai::Controller>(world); }

void NavVolume::GetArea(glm::vec2& minimum, glm::vec2& maximum)
{
    const glm::vec3 center = GetWorldPosition();
    const glm::vec3 size = glm::abs(GetScale());
    minimum = glm::vec2(center.x, center.z) - glm::vec2(size.x, size.z) * 0.5f;
    maximum = glm::vec2(center.x, center.z) + glm::vec2(size.x, size.z) * 0.5f;
}

namespace Ai
{
void SmartObject::BeginPlay()
{
    GameComponent::BeginPlay();
    if (Game* game = GetGame())
        if (auto* subsystem = game->GetSubsystem<Subsystem>()) subsystem->GetWorld().RegisterSmartObject(*this);
}

bool SmartObject::Claim(uint32_t controller)
{
    if (IsUsedBy(controller)) return true;
    if (FreeSlots() <= 0) return false;
    users_.push_back(controller);
    return true;
}

void SmartObject::Release(uint32_t controller) { users_.erase(std::remove(users_.begin(), users_.end(), controller), users_.end()); }
bool SmartObject::IsUsedBy(uint32_t controller) const { return std::find(users_.begin(), users_.end(), controller) != users_.end(); }
} // namespace Ai
