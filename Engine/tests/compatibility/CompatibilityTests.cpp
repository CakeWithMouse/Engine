#define NOMINMAX
#include <Windows.h>
#include "../../Runtime/ProjectUpgrade.h"
#include "../../Runtime/ProjectExport.h"
#include "../../../Game/Launcher/Source/ProjectLibrary.h"
#include <Engine/Runtime/EditExtensions.h>
#include <iostream>

namespace fs = std::filesystem;
using Json = nlohmann::json;
static int checks = 0;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); ++checks; }
template<class F> static void Reject(F&& f, const char* message) { bool rejected = false; try { f(); } catch (...) { rejected = true; } Check(rejected,message); }
static Json Starter()
{
    return {{"version",1},{"name","Old scene"},{"objects",Json::array({{{"id",1},{"name","Old Cube"},{"type","Primitive.Cube"},
        {"parent",0},{"inheritParentScale",true},{"transform",{{"position",{0,0,0}},{"rotation",{0,0,0,1}},{"scale",{1,1,1}}}}}})}};
}
static fs::path LegacyProject(const fs::path& fixture, const fs::path& root)
{
    fs::create_directories(root / "Content/Scenes");
    for (const auto& entry : fs::directory_iterator(fixture)) if (entry.path().extension() == ".dll") fs::copy_file(entry.path(),root / entry.path().filename());
    ProjectUpgrade::WriteJson(root / "Project.json",{{"name","Legacy"},{"module","Game.dll"},{"content","Content"},{"startupScene","Content/Scenes/Main.scene.json"}});
    ProjectUpgrade::WriteJson(root / "Content/Scenes/Main.scene.json",Starter());
    return root / "Project.json";
}
static void ExerciseModule(const fs::path& file, const fs::path& runtime, bool lights)
{
    const auto project = ReadRuntimeProject(file);
    const auto beforeHash = ProjectUpgrade::Hash(project.module);
    HWND window = CreateWindowExW(0,L"STATIC",L"Compatibility",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    struct Window { HWND h; ~Window() { DestroyWindow(h); } } guard{window};
    GameEditModule module(project.module,runtime / "EngineContent",project.content);
    const auto& api = module.Api(); char error[2048]{}; uint64_t id = 0;
    Check(bool(api.scene.setPointLight) == lights,"legacy light capability is absent; current capability is present");
    if (module.PreferredSceneVersion() == 2)
    {
        const auto library = LoadLibraryExW(project.module.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(library != nullptr,"inspect extension export");
        struct Library { HMODULE h; ~Library() { FreeLibrary(h); } } libraryGuard{library};
        const auto query = reinterpret_cast<EngineGetEditExtensionFn>(GetProcAddress(library,"EngineGetEditExtension"));
        EnginePointLightApi capability{99,nullptr,nullptr};
        Check(query && !query(api.context,"Unknown",1,sizeof(capability),&capability) && capability.version == 99,"unknown extension leaves output unchanged");
        Check(!query(api.context,EnginePointLightExtension,2,sizeof(capability),&capability) && capability.version == 99,"unknown extension version rejected");
        Check(!query(api.context,EnginePointLightExtension,1,sizeof(capability)-1,&capability) && capability.version == 99,"extension size checked");
        Check(query(api.context,EnginePointLightExtension,1,sizeof(capability),&capability) && capability.version == 1 && capability.set,"independent light extension query");
    }
    Check(api.loadScene(api.context,project.startupScene.u8string().c_str(),&id,error,sizeof(error)) == EngineSceneOk,"load frozen release scene");
    EngineSceneTransform t{{1,2,3},{0,0,0,1},{1,1,1}};
    Check(api.scene.createObject(api.scene.context,"Primitive.Cube","Authored",&t,&id) == EngineSceneOk,"create through negotiated ABI");
    t.position[0] = 7;
    Check(api.scene.setTransform(api.scene.context,id,&t) == EngineSceneOk,"edit frozen DLL object");
    Check(api.scene.renameObject(api.scene.context,id,"Renamed") == EngineSceneOk,"rename frozen DLL object");
    if (lights) {
        uint64_t lightId=0;
        const EngineScenePointLight light{{0.2f,0.7f,0.4f},3.0f,12.0f,1};
        Check(api.scene.createObject(api.scene.context,"Light.Point","Adapter light",&t,&lightId) == EngineSceneOk, "create light through negotiated ABI");
        Check(api.scene.setPointLight(api.scene.context,lightId,&light) == EngineSceneOk, "edit light through negotiated ABI");
        EngineSceneInfo lightInfo{}; bool lightFound=false;
        Check(api.scene.readScene(api.scene.context,&lightInfo,[](void* p,const EngineSceneObjectInfo* o){
            if(std::string(o->name)=="Adapter light") *static_cast<bool*>(p)=o->hasPointLight && o->pointLight.intensity==3 && o->pointLight.radius==12;
        },&lightFound)==EngineSceneOk && lightFound,"point light snapshot fields survive ABI translation");
    }
    const auto saved = project.content / "Scenes/Saved.scene.json";
    Check(api.saveScene(api.context,saved.u8string().c_str(),error,sizeof(error)) == EngineSceneOk,"save old DLL data");
    EngineSceneInfo info{}; bool found = false;
    auto visitor = [](void* p, const EngineSceneObjectInfo* o) {
        if (std::string(o->name) == "Renamed") *static_cast<bool*>(p) = o->transform.position[0] == 7 && !o->hasPointLight && o->modelAsset && o->textureAsset;
    };
    Check(api.scene.readScene(api.scene.context,&info,visitor,&found) == EngineSceneOk && found,"translate old snapshot fields safely");
    Check(window && api.attachViewport(api.context,window,640,480) == EngineSceneOk,"legacy viewport attach");
    Check(api.frame(api.context,0.016) == EngineSceneOk,"render old project");
    Check(api.control(api.context,EnginePlay) == EngineSceneOk,"Play frozen module");
    Check(api.frame(api.context,0.016) == EngineSceneOk,"render old Play copy");
    Check(api.scene.setTransform(api.scene.context,id,&t) == EngineSceneReadOnly,"legacy read-only contract");
    Check(api.control(api.context,EngineStop) == EngineSceneOk,"Stop frozen module");
    Check(api.loadScene(api.context,saved.u8string().c_str(),&id,error,sizeof(error)) == EngineSceneOk,"reopen old saved scene");
    if (!api.setMaterial) {
        Check(!api.readMaterial && !api.attachMaterialPreview, "legacy DLL does not advertise unsupported materials");
        const auto candidate = project.module.parent_path() / "ReloadCandidate.dll";
        fs::copy_file(project.module, candidate);
        const auto previousContext = api.context;
        Check(module.Reload(candidate, saved, window, 640, 480, error, sizeof(error)) == EngineSceneOk, "reload frozen module through adapter");
        Check(api.context != previousContext, "reloaded legacy session has a new identity for the editor");
        found = false;
        Check(api.readActiveScene(api.context,&info,visitor,&found) == EngineSceneOk && found, "legacy adapter still owns its context after reload");
        Check(api.frame(api.context,0.016) == EngineSceneOk, "legacy viewport works after reload");
    }
    Check(ProjectUpgrade::Hash(project.module) == beforeHash,"frozen game DLL never modified");
}
int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 5,"CompatibilityTests runtime pre-lights-fixture lights-fixture work");
        const auto runtime = fs::absolute(argv[1]), root = fs::absolute(argv[4]);
        fs::create_directories(root);
        SetEnvironmentVariableW(L"PAINENGINE_ENGINE_STORE",(root / "Engines").c_str());
        const auto release = ReadEngineRelease(runtime);
        const auto old = LegacyProject(fs::absolute(argv[2]),root / "Old project");
        const auto lights = LegacyProject(fs::absolute(argv[3]),root / "Light project");
        ExerciseModule(old,runtime,false); ExerciseModule(lights,runtime,true);
        for (const auto& fixture : {std::make_pair(old,"2026.10.24.0"),std::make_pair(lights,"2026.10.24.1")}) {
            const auto lock=fixture.first.parent_path()/"Engine.lock.json";
            ProjectUpgrade::WriteJson(lock,{{"format",1},{"engineVersion",fixture.second},{"engineConfiguration","Release"}});
            const auto projectHash=ProjectUpgrade::Hash(fixture.first), lockHash=ProjectUpgrade::Hash(lock), dllHash=ProjectUpgrade::Hash(fixture.first.parent_path()/"Game.dll");
            Reject([&] { ProjectExport::CheckProcess(runtime/"EngineRuntime.exe",L"--project \""+fixture.first.wstring()+L"\" --mode game --frames 3",fixture.first.parent_path()/"pinned.log"); },"ordinary launch still respects engine lock");
            for(const auto* mode:{L"game",L"editor"}) {
                const auto args=L"--project \""+fixture.first.wstring()+L"\" --mode "+mode+L" --compatible-engine --frames 3"+(std::wstring(mode)==L"editor"?L" --scene-api":L"");
                ProjectExport::CheckProcess(runtime/"EngineRuntime.exe",args,fixture.first.parent_path()/(std::wstring(mode)+L"-current.log"));
                Check(true,"pinned historical game launches on new runtime");
            }
            Check(ProjectUpgrade::Hash(fixture.first)==projectHash && ProjectUpgrade::Hash(lock)==lockHash && ProjectUpgrade::Hash(fixture.first.parent_path()/"Game.dll")==dllHash,"compatible launch preserves project, lock and frozen DLL");
        }
        auto current = ProjectLibrary::Create(root / "New project","Current",runtime / "Templates/SceneProject/Game.dll");
        auto settings = ReadRuntimeProject(current);
        Check(settings.engineVersion == release.version && settings.engineConfiguration == release.configuration,"new project records engine release");
        Check(ResolveEngineRelease(settings,runtime).runtime == release.runtime,"resolve current release");
        const auto installed = InstallEngineRelease(runtime);
        Check(ResolveEngineRelease(settings,runtime).runtime == installed.runtime,"prefer preserved installed release");
        Check(InstallEngineRelease(runtime).runtime == installed.runtime,"installation is idempotent");
        ProjectUpgrade::Copy(current,root / "MigratedLights",runtime);
        const auto litScene = SceneDocument::Read(root / "MigratedLights/Content/Scenes/Main.scene.json");
        size_t lightCount = 0;
        for (const auto& object : litScene.components) if (object.pointLight) { ++lightCount; Check(object.pointLight->radius == 10,"light migration preserves radius"); }
        Check(litScene.documentVersion == 2 && lightCount == 2,"v2 versioned light properties round trip");
        settings.engineVersion = "missing-version";
        Reject([&] { ResolveEngineRelease(settings,runtime); },"missing pinned version is not silently substituted");
        const auto oldScene = ReadRuntimeProject(old).startupScene;
        const auto sceneHash = ProjectUpgrade::Hash(oldScene), dllHash = ProjectUpgrade::Hash(old.parent_path()/"Game.dll");
        ProjectUpgrade::Copy(old,root / "Migrated",runtime);
        const auto migrated = ReadRuntimeProject(root / "Migrated/Project.json");
        Check(migrated.engineVersion == release.version,"migrated copy pinned to target");
        Check(SceneDocument::Read(migrated.startupScene).documentVersion == 2,"v1 scene migrates to v2");
        Check(ProjectUpgrade::Hash(migrated.module) == ProjectUpgrade::Hash(runtime / "Templates/SceneProject/Game.dll"),"known template updated");
        Check(ProjectUpgrade::Hash(oldScene) == sceneHash && ProjectUpgrade::Hash(old.parent_path()/"Game.dll") == dllHash,"original preserved byte-for-byte");
        ExerciseModule(root / "Migrated/Project.json",runtime,true);
        Reject([&] { ProjectUpgrade::Copy(old,root / "Migrated",runtime); },"never overwrite existing destination");
        Reject([&] { ProjectUpgrade::Copy(old,old.parent_path()/"Nested",runtime); },"reject destination inside source");
        auto future = Starter(); future["objects"][0]["unknown"] = 123;
        ProjectUpgrade::WriteJson(old.parent_path()/"Content/Scenes/Future.scene.json",future);
        Reject([&] { ProjectUpgrade::Copy(old,root/"Rejected",runtime); },"unknown data aborts upgrade rather than disappearing");
        Check(!fs::exists(root/"Rejected") && ProjectUpgrade::Hash(oldScene) == sceneHash,"failed migration leaves original intact");
        fs::remove(old.parent_path()/"Content/Scenes/Future.scene.json");
        const auto custom = LegacyProject(fs::absolute(argv[2]),root/"Custom");
        { std::ofstream overlay(custom.parent_path()/"Game.dll",std::ios::binary|std::ios::app); overlay << "custom module overlay"; }
        const auto customHash = ProjectUpgrade::Hash(custom.parent_path()/"Game.dll");
        ProjectUpgrade::Copy(custom,root/"CustomCopy",runtime);
        Check(ProjectUpgrade::Hash(root/"CustomCopy/Game.dll") == customHash,"unrecognized custom DLL never replaced with template");
        Check(SceneDocument::Read(root/"CustomCopy/Content/Scenes/Main.scene.json").documentVersion == 1,"migration respects legacy module document capability");
        const auto v1 = SceneDocument::Parse(Starter().dump());
        const auto v2 = SceneDocument::Migrate(v1,2);
        Check(SceneDocument::Serialize(SceneDocument::Parse(SceneDocument::Serialize(v2))) == SceneDocument::Serialize(v2),"v2 round trip stable");
        Reject([&] { SceneDocument::Migrate(v2,1); },"no automatic downgrade");
        auto properties = Starter(); properties["version"] = 2;
        properties["objects"][0]["properties"] = {{"Unknown.Component",{{"version",1},{"value",42}}}};
        Reject([&] { SceneDocument::Parse(properties.dump()); },"unknown versioned properties rejected losslessly");
        std::cout << "CompatibilityTests: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "CompatibilityTests: " << e.what() << '\n'; return 1; }
}
