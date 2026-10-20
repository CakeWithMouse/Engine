#include "VfsService.h"
#include <Engine/Resources/VirtualFileSystem.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <vector>

namespace
{
#if defined(_WIN32)
#define VFS_SVC_STRICMP _stricmp
#define VFS_SVC_STRNICMP _strnicmp
#else
#define VFS_SVC_STRICMP strcasecmp
#define VFS_SVC_STRNICMP strncasecmp
#endif

EngineVfsStatus ResolveImpl(void* ctx, const char* vpath, char* outPhysical, uint64_t capacity, uint64_t* needed)
{
    if (!ctx || !vpath) return ENGINE_VFS_BAD_ARGUMENT;
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

EngineVfsStatus ToVirtualImpl(void* ctx, const char* physical, char* outVpath, uint64_t capacity, uint64_t* needed)
{
    if (!ctx || !physical) return ENGINE_VFS_BAD_ARGUMENT;
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
        const std::string absGeneric = absPath.generic_u8string();

        for (const auto& mount : vfs.GetMounts())
        {
            const std::string mountGen = mount.physicalPath.generic_u8string();
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

int ExistsImpl(void* ctx, const char* vpath)
{
    if (!ctx || !vpath) return 0;
    auto& vfs = *static_cast<VirtualFileSystem*>(ctx);
    return vfs.Exists(vpath) ? 1 : 0;
}

EngineVfsStatus StatImpl(void* ctx, const char* vpath, EngineVfsStat* outStat)
{
    if (!ctx || !vpath || !outStat) return ENGINE_VFS_BAD_ARGUMENT;
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

EngineVfsStatus ReadFileImpl(void* ctx, const char* vpath, EngineVfsAllocFn allocFn, void* allocUser, uint64_t* outReadBytes)
{
    if (!ctx || !vpath || !allocFn) return ENGINE_VFS_BAD_ARGUMENT;
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
    catch (...)
    {
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
    return ENGINE_VFS_OK;
}

EngineVfsStatus EnumerateImpl(void* ctx, const char* vdir, int recursive, EngineVfsEnumerateCallback callback, void* user)
{
    if (!ctx || !vdir || !callback) return ENGINE_VFS_BAD_ARGUMENT;
    auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

    // Snapshot entries to avoid holding internal mutex during callbacks
    const std::vector<VirtualFileInfo> entries = vfs.EnumerateFiles(vdir, recursive != 0);
    for (const auto& entry : entries)
    {
        const int cont = callback(user, entry.virtualPath.c_str(), ENGINE_VFS_TYPE_FILE, static_cast<uint64_t>(entry.sizeBytes));
        if (!cont) break;
    }
    return ENGINE_VFS_OK;
}

EngineVfsStatus FindImpl(void* ctx, const char* vdir, uint32_t typeFilter, const char* pattern, int recursive, EngineVfsFindCallback callback, void* user)
{
    if (!ctx || !vdir || !callback) return ENGINE_VFS_BAD_ARGUMENT;
    auto& vfs = *static_cast<VirtualFileSystem*>(ctx);

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

EngineVfsStatus GetMountsImpl(void* ctx, EngineVfsMountCallback callback, void* user)
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

EngineVfsStatus WriteFileStub(void*, const char*, const void*, uint64_t, int) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus CreateDirStub(void*, const char*, int) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus MoveFileStub(void*, const char*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus CopyFileStub(void*, const char*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus DeleteFileStub(void*, const char*) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus SubscribeStub(void*, const char*, EngineVfsChangeCallback, void*, uint64_t*) { return ENGINE_VFS_NOT_SUPPORTED; }
EngineVfsStatus UnsubscribeStub(void*, uint64_t) { return ENGINE_VFS_NOT_SUPPORTED; }
}

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
