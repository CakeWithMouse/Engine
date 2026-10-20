#include "VfsService.h"
#include <Engine/Resources/VirtualFileSystem.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VFS_SVC_STRICMP _stricmp
#define VFS_SVC_STRNICMP _strnicmp
#else
#define VFS_SVC_STRICMP strcasecmp
#define VFS_SVC_STRNICMP strncasecmp
#endif

namespace
{
static bool IsValidUtf8(const char* str)
{
    if (!str) return false;
    const size_t len = std::strlen(str);
    if (len == 0) return true;
#if defined(_WIN32)
    const int res = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, static_cast<int>(len), nullptr, 0);
    return res > 0;
#else
    return true;
#endif
}

uint32_t ENGINE_MODULE_CALL ResolveImpl(void* ctx, const char* vpath, char* outPhysical, uint64_t capacity, uint64_t* needed)
{
    try
    {
        if (!ctx || !vpath || !IsValidUtf8(vpath)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        const auto resolved = vfs.Resolve(vpath);
        if (!resolved)
        {
            if (needed) *needed = 0;
            return ENGINE_VFS_NOT_FOUND;
        }

        const std::string phys = resolved->generic_u8string();
        const uint64_t req = static_cast<uint64_t>(phys.size() + 1);
        if (needed) *needed = req;

        if (!outPhysical || capacity < req)
        {
            return ENGINE_VFS_BUFFER_TOO_SMALL;
        }

        std::memcpy(outPhysical, phys.c_str(), static_cast<size_t>(req));
        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] Resolve exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] Resolve unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL ToVirtualImpl(void* ctx, const char* physical, char* outVpath, uint64_t capacity, uint64_t* needed)
{
    try
    {
        if (!ctx || !physical || !IsValidUtf8(physical)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        std::string vpath;
        bool found = false;

        // Check if input is already formatted as virtual path
        std::string rawStr = physical;
        for (char& c : rawStr) { if (c == '\\') c = '/'; }
        if (!rawStr.empty() && rawStr.front() == '/')
        {
            const std::string norm = VirtualFileSystem::NormalizeVirtualPath(rawStr);
            if (vfs.Exists(norm))
            {
                vpath = norm;
                found = true;
            }
            else
            {
                for (const auto& mount : vfs.GetMounts())
                {
                    if (norm == mount.virtualRoot ||
                        (norm.size() > mount.virtualRoot.size() &&
                         norm[mount.virtualRoot.size()] == '/' &&
                         VFS_SVC_STRNICMP(norm.c_str(), mount.virtualRoot.c_str(), mount.virtualRoot.size()) == 0))
                    {
                        vpath = norm;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            std::error_code ec;
            std::filesystem::path p = std::filesystem::u8path(physical);
            if (!p.is_absolute())
            {
                p = std::filesystem::absolute(p, ec);
            }
            const auto absPath = p.lexically_normal();
            std::string absGeneric = absPath.generic_u8string();

            for (const auto& mount : vfs.GetMounts())
            {
                std::string mountGen = mount.physicalPath.generic_u8string();
                const size_t mLen = mountGen.size();
                if (absGeneric.size() >= mLen &&
                    VFS_SVC_STRNICMP(absGeneric.c_str(), mountGen.c_str(), mLen) == 0)
                {
                    if (absGeneric.size() == mLen)
                    {
                        vpath = mount.virtualRoot;
                        found = true;
                        break;
                    }
                    if (absGeneric[mLen] == '/')
                    {
                        vpath = VirtualFileSystem::NormalizeVirtualPath(mount.virtualRoot + "/" + absGeneric.substr(mLen + 1));
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                // Check relative and standard content root markers if the candidate exists in VFS
                if (VFS_SVC_STRNICMP(rawStr.c_str(), "Engine/", 7) == 0)
                {
                    const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/" + rawStr);
                    if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                }
                else if (VFS_SVC_STRNICMP(rawStr.c_str(), "EngineContent/", 14) == 0)
                {
                    const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/Engine/" + rawStr.substr(14));
                    if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                }
                else if (VFS_SVC_STRNICMP(rawStr.c_str(), "Game/", 5) == 0)
                {
                    const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/" + rawStr);
                    if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                }
                else if (VFS_SVC_STRNICMP(rawStr.c_str(), "GameContent/", 12) == 0)
                {
                    const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/Game/" + rawStr.substr(12));
                    if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                }
            }

            if (!found)
            {
                // Check physical paths containing standard content markers if candidate exists in VFS
                const auto ecPos = absGeneric.rfind("/EngineContent/");
                if (ecPos != std::string::npos)
                {
                    const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/Engine/" + absGeneric.substr(ecPos + 15));
                    if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                }
                if (!found)
                {
                    const auto cPos = absGeneric.rfind("/Engine/Content/");
                    if (cPos != std::string::npos)
                    {
                        const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/Engine/" + absGeneric.substr(cPos + 16));
                        if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                    }
                }
                if (!found)
                {
                    const auto gcPos = absGeneric.rfind("/GameContent/");
                    if (gcPos != std::string::npos)
                    {
                        const std::string candidate = VirtualFileSystem::NormalizeVirtualPath("/Game/" + absGeneric.substr(gcPos + 13));
                        if (vfs.Exists(candidate)) { vpath = candidate; found = true; }
                    }
                }
            }
        }

        if (!found)
        {
            if (needed) *needed = 0;
            return ENGINE_VFS_NOT_FOUND;
        }

        const uint64_t req = static_cast<uint64_t>(vpath.size() + 1);
        if (needed) *needed = req;

        if (!outVpath || capacity < req)
        {
            return ENGINE_VFS_BUFFER_TOO_SMALL;
        }

        std::memcpy(outVpath, vpath.c_str(), static_cast<size_t>(req));
        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] ToVirtual exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] ToVirtual unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL ExistsImpl(void* ctx, const char* vpath)
{
    try
    {
        if (!ctx || !vpath || !IsValidUtf8(vpath)) return 0;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);
        return vfs.Exists(vpath) ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] Exists exception: %s\n", e.what());
        return 0;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] Exists unknown exception\n");
        return 0;
    }
}

uint32_t ENGINE_MODULE_CALL StatImpl(void* ctx, const char* vpath, EngineVfsStat* outStat)
{
    try
    {
        if (!ctx || !vpath || !outStat || !IsValidUtf8(vpath)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        const auto info = vfs.GetFileInfo(vpath);
        if (info)
        {
            outStat->type = ENGINE_VFS_TYPE_FILE;
            outStat->reserved = 0;
            outStat->size = static_cast<uint64_t>(info->sizeBytes);
            std::error_code ec;
            const auto ftime = std::filesystem::last_write_time(info->physicalPath, ec);
            if (!ec)
            {
                const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                outStat->mtime = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count());
            }
            else
            {
                outStat->mtime = 0;
            }
            return ENGINE_VFS_OK;
        }

        // Check directory
        const auto resolved = vfs.Resolve(vpath);
        if (resolved)
        {
            std::error_code ec;
            if (std::filesystem::is_directory(*resolved, ec))
            {
                outStat->type = ENGINE_VFS_TYPE_DIRECTORY;
                outStat->reserved = 0;
                outStat->size = 0;
                const auto ftime = std::filesystem::last_write_time(*resolved, ec);
                if (!ec)
                {
                    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    outStat->mtime = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count());
                }
                else
                {
                    outStat->mtime = 0;
                }
                return ENGINE_VFS_OK;
            }
        }

        return ENGINE_VFS_NOT_FOUND;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] Stat exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] Stat unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL ReadFileImpl(void* ctx, const char* vpath, EngineVfsAllocFn allocFn, void* allocUser, uint64_t* outReadBytes)
{
    try
    {
        if (!ctx || !vpath || !allocFn || !IsValidUtf8(vpath)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        if (!vfs.Exists(vpath))
        {
            return ENGINE_VFS_NOT_FOUND;
        }

        std::vector<uint8_t> bytes;
        try
        {
            bytes = vfs.ReadBytes(vpath);
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "[VfsService] ReadBytes exception for %s: %s\n", vpath, e.what());
            return ENGINE_VFS_IO_ERROR;
        }

        const uint64_t size = static_cast<uint64_t>(bytes.size());
        if (outReadBytes) *outReadBytes = size;

        void* dest = allocFn(allocUser, size);
        if (size > 0 && !dest)
        {
            return ENGINE_VFS_ALLOC_FAILED;
        }

        if (size > 0)
        {
            std::memcpy(dest, bytes.data(), static_cast<size_t>(size));
        }

        static std::atomic<bool> s_firstReadLogged{false};
        bool expected = false;
        if (s_firstReadLogged.compare_exchange_strong(expected, true))
        {
            std::printf("[VfsService] first read via module: %s\n", vpath);
        }

        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] ReadFile exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] ReadFile unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL EnumerateImpl(void* ctx, const char* vdir, int recursive, EngineVfsEnumerateCallback callback, void* user)
{
    try
    {
        if (!ctx || !vdir || !callback || !IsValidUtf8(vdir)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        if (!vfs.Exists(vdir))
        {
            return ENGINE_VFS_NOT_FOUND;
        }

        // Snapshot entries to avoid holding internal mutex during callbacks
        const std::vector<VirtualFileInfo> entries = vfs.EnumerateFiles(vdir, recursive != 0);
        for (const auto& entry : entries)
        {
            const int cont = callback(user, entry.virtualPath.c_str(), ENGINE_VFS_TYPE_FILE, static_cast<uint64_t>(entry.sizeBytes));
            if (!cont) break;
        }
        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] Enumerate exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] Enumerate unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL FindImpl(void* ctx, const char* vdir, uint32_t typeFilter, const char* pattern, int recursive, EngineVfsFindCallback callback, void* user)
{
    try
    {
        if (!ctx || !vdir || !callback || !IsValidUtf8(vdir)) return ENGINE_VFS_BAD_ARGUMENT;
        if (pattern && !IsValidUtf8(pattern)) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        if (!vfs.Exists(vdir))
        {
            return ENGINE_VFS_NOT_FOUND;
        }

        const std::string pat = pattern ? pattern : "";
        // Snapshot entries to avoid holding internal mutex during callbacks
        const std::vector<VirtualFileInfo> entries = vfs.FindFiles(vdir, AssetType::Unknown, pat, recursive != 0);
        for (const auto& entry : entries)
        {
            if (typeFilter != 0 && typeFilter != ENGINE_VFS_TYPE_FILE) continue;
            const int cont = callback(user, entry.virtualPath.c_str(), ENGINE_VFS_TYPE_FILE, static_cast<uint64_t>(entry.sizeBytes));
            if (!cont) break;
        }
        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] Find exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] Find unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL GetMountsImpl(void* ctx, EngineVfsMountCallback callback, void* user)
{
    try
    {
        if (!ctx || !callback) return ENGINE_VFS_BAD_ARGUMENT;
        auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

        // Snapshot mounts to avoid holding internal mutex during callbacks
        const std::vector<VirtualFileSystem::MountPoint> mounts = vfs.GetMounts();
        for (const auto& m : mounts)
        {
            const std::string phys = m.physicalPath.generic_u8string();
            const int cont = callback(user, m.virtualRoot.c_str(), phys.c_str());
            if (!cont) break;
        }
        return ENGINE_VFS_OK;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[VfsService] GetMounts exception: %s\n", e.what());
        return ENGINE_VFS_IO_ERROR;
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsService] GetMounts unknown exception\n");
        return ENGINE_VFS_IO_ERROR;
    }
}

uint32_t ENGINE_MODULE_CALL WriteFileStub(void*, const char*, const void*, uint64_t, int) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL CreateDirStub(void*, const char*, int) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL MoveFileStub(void*, const char*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL CopyFileStub(void*, const char*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL DeleteFileStub(void*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL SubscribeStub(void*, const char*, EngineVfsChangeCallback, void*, uint64_t*) { return ENGINE_VFS_NOT_SUPPORTED; }
uint32_t ENGINE_MODULE_CALL UnsubscribeStub(void*, uint64_t) { return ENGINE_VFS_NOT_SUPPORTED; }
} // namespace

EngineVfsApi CreateVfsService(VirtualFileSystem& vfs)
{
    EngineVfsApi api{};
    api.header.version = ENGINE_VFS_API_VERSION;
    api.header.size = sizeof(EngineVfsApi);
    api.context = &vfs;

    api.resolve = ResolveImpl;
    api.toVirtual = ToVirtualImpl;
    api.exists = ExistsImpl;
    api.stat = StatImpl;
    api.readFile = ReadFileImpl;
    api.enumerate = EnumerateImpl;
    api.find = FindImpl;
    api.getMounts = GetMountsImpl;

    api.writeFile = WriteFileStub;
    api.createDir = CreateDirStub;
    api.moveFile = MoveFileStub;
    api.copyFile = CopyFileStub;
    api.deleteFile = DeleteFileStub;

    api.subscribe = SubscribeStub;
    api.unsubscribe = UnsubscribeStub;

    return api;
}
