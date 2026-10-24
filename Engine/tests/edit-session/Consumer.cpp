#include <Engine/Runtime/EditSessionApi.h>
#include <new>
#include <exception>
#include <cstring>
#include <filesystem>
#include <fstream>
// ABI-only test consumer. No editor code, Qt, windows or input handling.
struct Consumer { EngineEditSessionApi api{}; bool bound = false; unsigned frames = 0; bool startup = false; };
static void* __cdecl Create(const EngineModuleHost* host)
{
    auto* s = new (std::nothrow) Consumer;
    if (s) s->startup = host->projectName && std::strcmp(host->projectName, "startup-scene-test") == 0;
    if (s && s->startup) std::ofstream(std::filesystem::u8path(std::string(host->projectFile) + ".consumer-created")) << "created";
    return s;
}
static bool Loaded(const EngineEditSessionApi& api)
{
    EngineSceneInfo info{}; bool found = false;
    auto visitor = [](void* p, const EngineSceneObjectInfo* o) {
        *static_cast<bool*>(p) = std::strcmp(o->name, "Startup object") == 0 &&
            std::strcmp(o->typeName, "RuntimeExample::Mover") == 0 && o->transform.position[0] == 5;
    };
    return api.scene.readScene(api.scene.context, &info, visitor, &found) == EngineSceneOk && info.objectCount == 1 && found;
}
static int __cdecl Start(void* p)
{
    auto& s = *static_cast<Consumer*>(p);
    if (!s.bound) return 0;
    uint32_t state = 99; uint64_t id = 0;
    if (s.api.getState(s.api.context, &state) != EngineSceneOk || state != EngineEditing) return 0;
    if (s.startup) return Loaded(s.api);
    return s.api.scene.createObject(s.api.scene.context, "RuntimeExample::Mover", "From consumer", nullptr, &id) == EngineSceneOk;
}
static int __cdecl Update(void* p, double)
{
    auto& s = *static_cast<Consumer*>(p);
    const uint32_t command = s.frames++ == 0 ? EnginePlay : s.frames == 2 ? EnginePause : EngineStop;
    if (s.api.control(s.api.context, command) != EngineSceneOk) return 0;
    return !s.startup || Loaded(s.api); // original edit state survives Play/Pause/Stop
}
static int __cdecl Overlay(void*) { return 1; }
static void __cdecl Stop(void* p) { if (static_cast<Consumer*>(p)->bound) std::terminate(); }
static void __cdecl Destroy(void* p) { delete static_cast<Consumer*>(p); }
extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (version != ENGINE_MODULE_VERSION || !api) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_EDITOR, Create, Start, Update, Overlay, Stop, Destroy};
    return 1;
}
extern "C" __declspec(dllexport) int __cdecl EngineBindEditSession(uint32_t version, uint32_t size, void* p, const EngineEditSessionApi* api)
{
    if (version != ENGINE_EDIT_API_VERSION || size != sizeof(EngineEditSessionApi) || !p) return 0;
    auto& s = *static_cast<Consumer*>(p);
    if (api && !IsCompleteEditSessionApi(*api)) return 0;
    if (api && s.startup && !Loaded(*api)) return 0; // must be ready BEFORE bind/start
    s.api = api ? *api : EngineEditSessionApi{}; s.bound = api != nullptr;
    return 1;
}
