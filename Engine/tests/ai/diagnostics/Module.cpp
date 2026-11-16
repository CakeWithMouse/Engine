#include <Engine/AI/Ai.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Project/StandardTypes.h>

// A deterministic, real gameplay fixture for editor diagnostics screenshots and regression checks.
class DiagnosticGuard final : public AiCharacter
{
  public:
    DiagnosticGuard()
    {
        sightRadius = 12;
        fieldOfView = 110;
        team = 1;
    }
    void SetupAi(Ai::Controller &c) override
    {
        c.SetRadius(.6f);
        c.SetSpeed(.4f);
        const Ai::Key<glm::vec3> goal{"Goal"};
        Ai::MachineBuilder machine("Diagnostic patrol");
        machine.State("Patrol").Task<Ai::Tasks::MoveTo>(goal, .5f);
        c.Memory().Set(goal, glm::vec3(12, 0, -8));
        c.Run(machine.Build());
        auto &world = c.GetWorld();
        auto &group = world.CreateGroup("North squad");
        for (const auto &e : world.GetScene()->GetComponents())
            if (e.first.rfind("Soldier", 0) == 0)
                if (auto *soldier = dynamic_cast<Character *>(e.second.get()))
                    group.AddCharacter(*soldier, .4f);
        Ai::Formation formation;
        formation.columns = 3;
        formation.spacing = 1.5f;
        group.SetFormation(formation);
        group.MoveTo({12, 0, 4});
    }
};
static ComponentTypeCatalog ProjectComponentTypes()
{
    static std::vector<ComponentType> types;
    if (types.empty())
    {
        auto base = SceneProject::GetComponentTypes();
        types.assign(base.data, base.data + base.size);
        types.push_back({"Test.DiagnosticGuard", "Diagnostic guard", EngineComponentDetail::Create<DiagnosticGuard>});
    }
    return {types.data(), types.size()};
}
#include <Engine/Project/SceneModule.inl>
