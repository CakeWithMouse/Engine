#pragma once

#include "IVfs.h"
#include "HostVfsProxy.h"
#include "LocalVfs.h"
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/VfsApi.h>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace VfsRead
{
    inline bool Bind(const EngineVfsApi* api)
    {
        return HostVfsProxy::Instance().Bind(api);
    }

    inline void Unbind()
    {
        HostVfsProxy::Instance().Unbind();
    }

    inline bool IsBound()
    {
        return HostVfsProxy::Instance().IsBound();
    }

    inline IVfs& Active()
    {
        if (HostVfsProxy::Instance().IsBound())
        {
            return HostVfsProxy::Instance();
        }

        static std::once_flag s_fallbackOnce;
        std::call_once(s_fallbackOnce, []() {
            std::printf("[VfsRead] using local VFS (no host service)\n");
        });

        return LocalVfs::Instance();
    }

    std::filesystem::path FindRepoRoot(const std::filesystem::path& start = {});
    VirtualFileSystem& GetVFS();
    std::string ToVirtualPath(VirtualFileSystem& vfs, std::string_view path);
    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr);
}
