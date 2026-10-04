#include <Engine/Runtime/ModuleApi.h>
#include <new>

// Separate test DLL binaries, never switches in the shipped game/editor module.
// 1 bad version, 2 wrong kind, 3 missing export, 4/5 failed game/editor start,
// 6 request exit after first update, 7 failed update.
#ifndef FIXTURE_CASE
#error FIXTURE_CASE must be set by the fixture project
#endif
namespace
{
struct Session { EngineModuleHost host; };
void Log(void* p, const char* event) { auto* s = static_cast<Session*>(p); s->host.log(s->host.user, event); }
void* __cdecl Create(const EngineModuleHost* host)
{
    auto* s = new (std::nothrow) Session{*host};
    if (s) Log(s, "FixtureCreate");
    return s;
}
int __cdecl Start(void* p) { Log(p, "FixtureStart"); return FIXTURE_CASE == 4 || FIXTURE_CASE == 5 ? 0 : 1; }
int __cdecl Update(void* p, double)
{
    Log(p, "FixtureUpdate");
    if (FIXTURE_CASE == 6) { auto* s = static_cast<Session*>(p); s->host.requestExit(s->host.user); }
    return FIXTURE_CASE == 7 ? 0 : 1;
}
int __cdecl Overlay(void*) { return 1; }
void __cdecl Stop(void* p) { Log(p, "FixtureStop"); }
void __cdecl Destroy(void* p) { Log(p, "FixtureDestroy"); delete static_cast<Session*>(p); }
}
#if FIXTURE_CASE == 3
extern "C" __declspec(dllexport) int __cdecl DifferentExport() { return 0; }
#else
extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {FIXTURE_CASE == 1 ? 999u : ENGINE_MODULE_VERSION,
        FIXTURE_CASE == 2 || FIXTURE_CASE == 5 ? ENGINE_MODULE_EDITOR : ENGINE_MODULE_GAME,
        Create, Start, Update, Overlay, Stop, Destroy};
    return 1;
}
#endif
