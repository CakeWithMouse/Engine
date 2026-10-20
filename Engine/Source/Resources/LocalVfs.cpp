#include "LocalVfs.h"
#include "HostVfsProxy.h"
#include "VfsRead.h"
#include <cstdio>
#include <exception>
#include <mutex>

namespace
{
struct FallbackAutoRegistrar
{
    FallbackAutoRegistrar()
    {
        HostVfsProxy::Instance().SetFallbackVfs(&LocalVfs::Instance());
    }
} s_registrar;
} // namespace

LocalVfs& LocalVfs::Instance()
{
    static LocalVfs s_instance;
    HostVfsProxy::Instance().SetFallbackVfs(&s_instance);
    return s_instance;
}

VirtualFileSystem& LocalVfs::GetUnderlyingVfs()
{
    return VfsRead::GetVFS();
}

bool LocalVfs::Exists(std::string_view virtualPath) const
{
    return VfsRead::GetVFS().Exists(virtualPath);
}

std::string LocalVfs::ToVirtualPath(std::string_view path) const
{
    return VfsRead::ToVirtualPath(VfsRead::GetVFS(), path);
}

std::vector<uint8_t> LocalVfs::ReadBytes(std::string_view path, std::string* outVirtualPath) const
{
    auto& vfs = VfsRead::GetVFS();
    const std::string vPath = ToVirtualPath(path);
    if (outVirtualPath)
    {
        *outVirtualPath = vPath;
    }

    try
    {
        return vfs.ReadBytes(vPath);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "[VfsRead] Failed to read bytes for path '%s' (virtual: '%s'): %s\n",
            std::string(path).c_str(), vPath.c_str(), ex.what());
        return {};
    }
    catch (...)
    {
        std::fprintf(stderr, "[VfsRead] Failed to read bytes for path '%s' (virtual: '%s'): unknown exception\n",
            std::string(path).c_str(), vPath.c_str());
        return {};
    }
}
