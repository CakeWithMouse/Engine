#define NOMINMAX
#include <Windows.h>
#include <Engine/Runtime/GameEditModule.h>
#include <Engine/Scene/SceneDocument.h>
#include "../../../Game/Launcher/Source/ProjectLibrary.h"
#include "../../../Game/SceneProject/Source/Primitives.h"
#include <iostream>

static int checks = 0;
static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
    ++checks;
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 4, "usage: ProjectWorkflowTests <template Game.dll> <EngineContent> <new test directory>");
        const auto root = std::filesystem::absolute(argv[3]);
        const auto file = ProjectLibrary::Create(root, "Проверка проекта Unicode", std::filesystem::absolute(argv[1]));
        auto project = ProjectLibrary::Validate(file);
        Check(project.name == "Проверка проекта Unicode", "UTF-8 project name");
        Check(project.content == root / "Content" && project.module == root / "Game.dll", "isolated project roots");
        Check(SceneDocument::Read(project.startupScene).components.size() == 2, "starter floor and cube");
        bool rejected = false;
        try { ProjectLibrary::Create(root, "Overwrite", argv[1]); } catch (...) { rejected = true; }
        Check(rejected && ProjectLibrary::Validate(file).name == project.name, "existing project never overwritten");
        rejected = false;
        try { ProjectLibrary::Create(root / "bad", "  ", argv[1]); } catch (...) { rejected = true; }
        Check(rejected && !std::filesystem::exists(root / "bad"), "invalid creation leaves no project");

        HWND window = CreateWindowExW(0, L"STATIC", L"Scene project test", WS_OVERLAPPEDWINDOW,
            0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window != nullptr, "test viewport");
        struct WindowGuard { HWND window; ~WindowGuard() { DestroyWindow(window); } } guard{window};
        {
            GameEditModule module(project.module, std::filesystem::absolute(argv[2]));
            const auto& api = module.Api(); char error[1024]{}; uint64_t scene = 0;
            Check(api.loadScene(api.context, project.startupScene.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "load starter scene");
            std::vector<std::string> types;
            Check(api.scene.listTypes(api.scene.context, [](void* p, const EngineSceneTypeInfo* t) {
                static_cast<std::vector<std::string>*>(p)->push_back(t->typeName);
            }, &types) == EngineSceneOk && types.size() == 6, "six registered primitive types");
            Check(api.attachViewport(api.context, window, 640, 480) == EngineSceneOk, "attach real D3D viewport");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "initial frame");
            uint64_t picked = 0;
            Check(api.pickObject(api.context, 0, 0, &picked) == EngineSceneOk && picked, "pick starter cube at viewport center");
            Check(api.focusObject(api.context, picked) == EngineSceneOk, "focus selected object");
            Check(api.pickObject(api.context, 2, 0, &picked) == EngineSceneInvalidArgument && !picked, "reject invalid viewport coordinate");
            int index = 0; uint64_t first = 0;
            for (const auto& type : types)
            {
                EngineSceneTransform transform{{static_cast<float>(index++),1,0},{0,0.38268343f,0,0.92387953f},{1,2,1}};
                uint64_t id = 0;
                Check(api.scene.createObject(api.scene.context, type.c_str(), type.c_str(), &transform, &id) == EngineSceneOk && id, "create primitive after attach");
                if (!first) first = id;
                Check(api.frame(api.context, 0.016) == EngineSceneOk, "render newly added primitive");
            }
            EngineSceneTransform changed{{-2,3,-1},{0,0,0,1},{2,3,4}};
            Check(api.scene.setTransform(api.scene.context, first, &changed) == EngineSceneOk, "edit transform");
            Check(api.scene.renameObject(api.scene.context, first, "Изменённый куб") == EngineSceneOk, "rename object");
            uint64_t duplicate = 0;
            Check(api.scene.createObject(api.scene.context, "Primitive.Cube", "Изменённый куб", nullptr, &duplicate) == EngineSceneDuplicateName && !duplicate, "duplicate name rejected");
            Check(api.scene.createObject(api.scene.context, "Primitive.Cube", "Temporary", nullptr, &duplicate) == EngineSceneOk, "duplicate object");
            Check(api.scene.removeObject(api.scene.context, duplicate) == EngineSceneOk, "delete object");
            Check(api.saveScene(api.context, project.startupScene.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "save authored scene");
            const auto saved = SceneDocument::Read(project.startupScene);
            Check(saved.components.size() == 8, "all primitives persisted");
            Check(api.control(api.context, EnginePlay) == EngineSceneOk, "play authored scene");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render play copy");
            Check(api.scene.removeObject(api.scene.context, first) == EngineSceneReadOnly, "edit locked in play");
            Check(api.control(api.context, EnginePause) == EngineSceneOk, "pause");
            Check(api.control(api.context, EngineResume) == EngineSceneOk, "resume");
            Check(api.control(api.context, EngineStop) == EngineSceneOk, "stop");
            Check(api.loadScene(api.context, project.startupScene.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneOk, "reopen saved scene");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render reopened scene");
            const auto invalid = root / "invalid.scene.json";
            ProjectLibrary::Write(invalid, {{"version",99},{"name","invalid"},{"objects",ProjectLibrary::Json::array()}});
            Check(api.loadScene(api.context, invalid.u8string().c_str(), &scene, error, sizeof(error)) == EngineSceneUnsupportedVersion, "reject invalid scene without replacing current scene");
            Check(api.frame(api.context, 0.016) == EngineSceneOk, "render preserved scene after failed load");
            const auto backup = root / "reopened.scene.json";
            Check(api.saveScene(api.context, backup.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "save reopened scene");
            auto reopened = SceneDocument::Read(backup);
            Check(reopened.components.size() == saved.components.size(), "round trip count");
            for (size_t i = 0; i < saved.components.size(); ++i)
            {
                const auto& a = saved.components[i]; const auto& b = reopened.components[i];
                Check(a.typeName == b.typeName && a.name == b.name && a.transform.position == b.transform.position &&
                    a.transform.scale == b.transform.scale && glm::length(a.transform.rotation - b.transform.rotation) < 1e-6f, "round trip authored properties");
            }
        }
        // The procedural catalog also produces drawable CPU meshes before any GPU initialization.
        for (const auto& type : {"Primitive.Cube", "Primitive.Sphere", "Primitive.Plane", "Primitive.Cylinder", "Primitive.Cone", "Primitive.Pyramid"})
        {
            const auto object = SceneProject::GetComponentTypes().Find(type)->create();
            Check(object->GetIndexCount() >= 6 && object->GetLocalBoundingRadius() > 0, "primitive geometry exists");
        }
        std::cout << "ProjectWorkflowTests: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "ProjectWorkflowTests: " << e.what() << '\n'; return 1; }
}
