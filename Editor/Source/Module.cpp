#include <Engine/Runtime/ModuleApi.h>
#include <cstdio>
#include <new>

namespace
{
struct Session { EngineModuleHost host; uint64_t updates = 0; };
void Log(Session* s, const char* text) { s->host.log(s->host.user, text); }
void* __cdecl Create(const EngineModuleHost* host)
{
    if (!host || host->version != ENGINE_MODULE_VERSION || !host->log || !host->requestExit) return nullptr;
    auto* s = new (std::nothrow) Session{*host};
    if (s) Log(s, "EditorCreate");
    return s;
}
int __cdecl Start(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    Log(s, "EditorStart");
    Log(s, "EditorOpenProject (stub)");
    Log(s, s->host.projectName);
    Log(s, s->host.projectFile);
    return 1;
}
int __cdecl Update(void* instance, double)
{
    auto* s = static_cast<Session*>(instance);
    ++s->updates;
    Log(s, "EditorUpdate");
    return 1;
}
int __cdecl Overlay(void* instance) { Log(static_cast<Session*>(instance), "EditorOverlay (stub)"); return 1; }
void __cdecl Stop(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    char count[96];
    std::snprintf(count, sizeof(count), "EditorUpdates=%llu", static_cast<unsigned long long>(s->updates));
    Log(s, count);
    Log(s, "EditorStop");
}
void __cdecl Destroy(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    Log(s, "EditorDestroy");
    delete s;
}
}
extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_EDITOR, Create, Start, Update, Overlay, Stop, Destroy};
    return 1;
}
