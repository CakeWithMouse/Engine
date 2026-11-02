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
        Check(catalog.size == 3, "discover all annotated classes, ignore strings/comments");
        Check(catalog.Find("Missing") == nullptr, "unknown type");
        const auto* type = catalog.Find("GenerationTest::Mover");
        Check(type && std::string_view(type->displayName) == "Moving object", "qualified name/display name");
        Check(!type->visualDefaults.geometryAsset && !type->visualDefaults.materialAsset, "visual placeholders");
        auto component = type->create();
        auto* mover = dynamic_cast<GenerationTest::Mover*>(component.get());
        Check(mover && mover->ticks == 0 && !mover->GetScene(), "factory creates real class without ticking");
        mover->LoadProjectData(R"({"version":1,"speed":9,"data":4,"mode":1,"title":"saved"})");
        Check(mover->speed==9 && mover->data==4 && !mover->DescribeEditorProperties().empty(), "generated class supports macro properties");
        Check(mover->Mode()==1 && mover->Title()=="saved", "private enum/string properties");
        Check(component->DescribeEditorProperties()==mover->DescribeEditorProperties(), "schema dispatch through GameComponent without provider");
        Check(!nlohmann::json::parse(mover->SaveProjectData()).contains("ticks"), "unannotated state is not saved");
        const auto saved=mover->SaveProjectData();
        try {mover->LoadProjectData(R"({"version":1,"speed":20,"mode":99})");throw std::runtime_error("invalid enum accepted");}catch(const std::invalid_argument&){}
        Check(mover->SaveProjectData()==saved,"invalid annotated field leaves data unchanged");
        auto second = type->create();
        Check(second.get() != component.get(), "independent instances");
        auto inherited=catalog.Find("GenerationTest::Inherited")->create();
        inherited->LoadProjectData(R"({"version":1,"speed":8,"mode":1,"extra":-1.5})");
        Check(nlohmann::json::parse(inherited->SaveProjectData())["extra"]==-1.5,"generated derived data");
        Check(nlohmann::json::parse(inherited->DescribeEditorProperties())["fields"].size()==5,"generated inherited schema");
        Check(dynamic_cast<GenerationTest::Mover*>(inherited.get())->speed==8,"generated base fields restored");
        second->LoadProjectData(R"({"version":1,"speed":6})");
        auto defaults=nlohmann::json::parse(second->SaveProjectData());
        Check(defaults["speed"]==6 && defaults["mode"]==0 && defaults["title"]=="default", "old data preserves new field defaults");
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
