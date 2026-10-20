#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/VfsApi.h>
#include "../../Engine/Source/Resources/VfsRead.h"
#include "QtHost.h"
#include <cstdio>
#include <new>

namespace
{
    void* __cdecl Create(const EngineModuleHost* host)
    {
        if (!host || host->version != ENGINE_MODULE_VERSION || !host->log || !host->requestExit) return nullptr;
        bool boundVfs = false;
        if (host->getService)
        {
            const auto* vfsApi = static_cast<const EngineVfsApi*>(
                host->getService(host->user, ENGINE_SERVICE_VFS, ENGINE_VFS_API_VERSION));
            if (vfsApi)
            {
                boundVfs = VfsRead::Bind(vfsApi);
            }
            else
            {
                std::fprintf(stderr, "[Editor] Host getService returned null for VFS service, falling back to local VFS\n");
            }
        }
        else
        {
            std::fprintf(stderr, "[Editor] Host getService is null, falling back to local VFS\n");
        }

        try {
            auto* e = new (std::nothrow) EditorQtHost(*host);
            if (e)
            {
                e->Log("EditorCreate");
                return e;
            }
        } catch (...) {}

        if (boundVfs)
        {
            VfsRead::Unbind();
        }
        return nullptr;
    }
    int __cdecl Start(void* i)   { auto* e = static_cast<EditorQtHost*>(i); if (!e) return 0; try { return e->Start() ? 1 : 0; } catch (...) { return 0; } }
    int __cdecl Update(void* i, double dt) { auto* e = static_cast<EditorQtHost*>(i); if (!e) return 0; try { e->Update(dt); return 1; } catch (...) { return 0; } }
    int __cdecl Overlay(void* i) { (void)i; return 1; } // Qt не immediate-mode: оверлей не нужен
    void __cdecl Stop(void* i)   { auto* e = static_cast<EditorQtHost*>(i); if (!e) return; try { e->Stop(); } catch (...) {} }
    void __cdecl Destroy(void* i)
    {
        auto* e = static_cast<EditorQtHost*>(i);
        if (!e) return;
        try {
            e->Log("EditorDestroy");
            delete e;
        } catch (...) {}
        VfsRead::Unbind();
    }
}

extern "C" __declspec(dllexport) int __cdecl EngineGetModuleApi(uint32_t version, EngineModuleApi* api)
{
    if (!api || version != ENGINE_MODULE_VERSION) return 0;
    *api = {ENGINE_MODULE_VERSION, ENGINE_MODULE_EDITOR, Create, Start, Update, Overlay, Stop, Destroy};
    return 1;
}