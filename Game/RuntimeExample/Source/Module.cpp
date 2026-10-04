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
    if (s) Log(s, "GameCreate");
    return s;
}
int __cdecl Start(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    Log(s, "GameStart");
    Log(s, s->host.projectName);
    return 1;
}
int __cdecl Update(void* instance, double)
{
    auto* s = static_cast<Session*>(instance);
    ++s->updates;
    Log(s, "GameUpdate");
    return 1;
}
void __cdecl Stop(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    char count[96];
    std::snprintf(count, sizeof(count), "GameUpdates=%llu", static_cast<unsigned long long>(s->updates));
    Log(s, count);
    Log(s, "GameStop");
}
void __cdecl Destroy(void* instance)
{
    auto* s = static_cast<Session*>(instance);
    Log(s, "GameDestroy");
    delete s;
}
}
// All operations are allocation-free except nothrow Create; no exceptions cross ABI.
extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_GAME, Create, Start, Update, nullptr, Stop, Destroy};
    return 1;
}
