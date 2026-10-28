#include "VfsRead.h"
#include "HostVfsProxy.h"
#include "LocalVfs.h"

#include <Engine/Resources/Paths.h>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>

namespace VfsRead
{
    static VirtualFileSystem s_vfs;
    static std::once_flag s_initOnce;

    std::filesystem::path FindRepoRoot(const std::filesystem::path& start)
    {
        std::error_code ec;
        auto cur = start.empty() ? EnginePaths::ExecutableDirectory() : start;
        for (int i = 0; i < 8 && !cur.empty(); ++i)
        {
            if (std::filesystem::exists(cur / ".git", ec) ||
                (std::filesystem::exists(cur / "Engine" / "Content", ec) &&
                 std::filesystem::exists(cur / "Engine" / "Engine.props", ec)))
            {
                return cur;
            }
            const auto parent = cur.parent_path();
            if (parent == cur) break;
            cur = parent;
        }
        return {};
    }

    VirtualFileSystem& GetVFS()
    {
        std::call_once(s_initOnce, []() {
            std::error_code ec;
            // 1. Mount /Engine
            auto engineContent = EnginePaths::ContentRoot();
            if (!std::filesystem::exists(engineContent, ec))
            {
                const auto repoRoot = FindRepoRoot();
                if (!repoRoot.empty() && std::filesystem::exists(repoRoot / "Engine" / "Content", ec))
                {
                    engineContent = (repoRoot / "Engine" / "Content").lexically_normal();
                }
                else
                {
                    auto execDir = EnginePaths::ExecutableDirectory();
                    if (std::filesystem::exists(execDir / "EngineContent", ec))
                    {
                        engineContent = (execDir / "EngineContent").lexically_normal();
                    }
                }
            }
            s_vfs.Mount("/Engine", engineContent);

            // 2. Mount /Game
            auto execDir = EnginePaths::ExecutableDirectory();
            auto gameContent = execDir / "GameContent";
            if (std::filesystem::exists(gameContent, ec))
            {
                s_vfs.Mount("/Game", gameContent);
            }
            else
            {
                const auto repoRoot = FindRepoRoot();
                if (!repoRoot.empty() && std::filesystem::exists(repoRoot / "Game" / "GameContent", ec))
                {
                    s_vfs.Mount("/Game", (repoRoot / "Game" / "GameContent").lexically_normal());
                }
            }
        });
        return s_vfs;
    }

    std::string ToVirtualPath(VirtualFileSystem& vfs, std::string_view path)
    {
        if (path.empty()) return "/";

        std::string pathStr(path);
        for (char& c : pathStr)
        {
            if (c == '\\') c = '/';
        }

        // If path starts with '/', check if it matches an existing mount prefix
        if (pathStr.front() == '/')
        {
            const auto norm = VirtualFileSystem::NormalizeVirtualPath(pathStr);
            if (vfs.Exists(norm))
            {
                return norm;
            }
            for (const auto& mount : vfs.GetMounts())
            {
                if (norm == mount.virtualRoot ||
                    (norm.size() > mount.virtualRoot.size() &&
                     norm[mount.virtualRoot.size()] == '/' &&
                     _strnicmp(norm.c_str(), mount.virtualRoot.c_str(), mount.virtualRoot.size()) == 0))
                {
                    return norm;
                }
            }
        }

        // Check if starts with "Engine/" or "EngineContent/"
        if (_strnicmp(pathStr.c_str(), "Engine/", 7) == 0)
        {
            return VirtualFileSystem::NormalizeVirtualPath("/" + pathStr);
        }
        if (_strnicmp(pathStr.c_str(), "EngineContent/", 14) == 0)
        {
            return VirtualFileSystem::NormalizeVirtualPath("/Engine/" + pathStr.substr(14));
        }

        // Check if starts with "Game/" or "GameContent/"
        if (_strnicmp(pathStr.c_str(), "Game/", 5) == 0)
        {
            return VirtualFileSystem::NormalizeVirtualPath("/" + pathStr);
        }
        if (_strnicmp(pathStr.c_str(), "GameContent/", 12) == 0)
        {
            return VirtualFileSystem::NormalizeVirtualPath("/Game/" + pathStr.substr(12));
        }

        // Check physical paths against existing mount points
        std::error_code ec;
        std::filesystem::path p(path);
        if (!p.is_absolute())
        {
            if (!std::filesystem::exists(p, ec))
            {
                auto fromExec = EnginePaths::ExecutableDirectory() / p;
                if (std::filesystem::exists(fromExec, ec))
                {
                    p = fromExec;
                }
            }
        }

        const auto absPath = std::filesystem::absolute(p, ec).lexically_normal();
        const std::string absGeneric = absPath.generic_u8string();

        for (const auto& mount : vfs.GetMounts())
        {
            const std::string mountGen = mount.physicalPath.generic_u8string();
            const size_t mLen = mountGen.size();
            if (absGeneric.size() >= mLen &&
                _strnicmp(absGeneric.c_str(), mountGen.c_str(), mLen) == 0)
            {
                if (absGeneric.size() == mLen)
                {
                    return mount.virtualRoot;
                }
                if (absGeneric[mLen] == '/')
                {
                    return VirtualFileSystem::NormalizeVirtualPath(mount.virtualRoot + "/" + absGeneric.substr(mLen + 1));
                }
            }
        }

        // If physical path exists outside registered mounts, mount its parent dynamically
        if (std::filesystem::exists(absPath, ec))
        {
            const auto parent = absPath.parent_path();
            const size_t h = std::hash<std::string>{}(parent.generic_u8string());
            const std::string vRoot = "/_ext_" + std::to_string(h);
            std::fprintf(stderr, "[VfsRead] Dynamic mount '%s' -> '%s' (path '%s' outside registered mounts)\n",
                vRoot.c_str(), parent.generic_u8string().c_str(), absGeneric.c_str());
            vfs.Mount(vRoot, parent);
            return vRoot + "/" + absPath.filename().generic_u8string();
        }

        // Fallback: treat as virtual path
        return VirtualFileSystem::NormalizeVirtualPath(pathStr.front() == '/' ? pathStr : ("/" + pathStr));
    }

    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath)
    {
        return Active().ReadBytes(path, outVirtualPath);
    }
}
#include <Engine/Runtime/ModuleVfs.h>
namespace EngineModuleVfs
{
    bool Bind(const EngineVfsApi* api) { return VfsRead::Bind(api); }
    void Unbind() { VfsRead::Unbind(); }
    std::string Resolve(const char* path)
    {
        if (VfsRead::IsBound()) return VfsRead::Resolve(path);
        const auto result = VfsRead::GetVFS().Resolve(path);
        return result ? result->u8string() : std::string{};
    }
}
