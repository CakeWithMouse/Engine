#pragma once
#include <Engine/Runtime/VfsApi.h>
#include <string>
// Module-local bridge implemented in Engine.lib; no private headers in game SDKs.
namespace EngineModuleVfs
{
    bool Bind(const EngineVfsApi* api);
    void Unbind();
    std::string Resolve(const char* path);
}
