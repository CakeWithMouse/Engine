#include "GeneratedComponents.h"
#include "TestComponents.h"
#include <Engine/Scene/Scene.h>
#include <iostream>
#include <stdexcept>

static void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
int main()
{
    try
    {
        const auto catalog = EngineGenerated::ComponentGenerationTests::GetComponentTypes();
        Check(catalog.size == 2, "discover all annotated classes, ignore strings/comments");
        Check(catalog.Find("Missing") == nullptr, "unknown type");
        const auto* type = catalog.Find("GenerationTest::Mover");
        Check(type && std::string_view(type->displayName) == "Moving object", "qualified name/display name");
        Check(!type->visualDefaults.geometryAsset && !type->visualDefaults.materialAsset, "visual placeholders");
        auto component = type->create();
        auto* mover = dynamic_cast<GenerationTest::Mover*>(component.get());
        Check(mover && mover->ticks == 0 && !mover->GetScene(), "factory creates real class without ticking");
        auto second = type->create();
        Check(second.get() != component.get(), "independent instances");
        Scene scene("Generated types");
        const auto added = scene.AddComponent("Mover 1", component);
        Check(added.result == SceneResult::Ok && !component, "Scene takes ownership");
        Check(mover->ticks == 0, "adding to Scene does not start simulation");
        scene.Tick(0.1f);
        Check(mover->ticks == 1, "Scene dispatches derived virtual Tick");
        const auto* indirect = catalog.Find("GenerationTest::Indirect");
        Check(indirect && indirect->create() && std::string_view(indirect->displayName) == indirect->typeName, "indirect inheritance/default label");
        Check(scene.RemoveComponent(added.id) == SceneResult::Ok, "remove generated object");
        std::cout << "Component generation integration: PASS (no editor/window/device)\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
