#include <Engine/Resources/VirtualFileSystem.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

#ifdef _WIN32
#include <string.h>
#define VFS_STRNICMP _strnicmp
#define VFS_STRICMP _stricmp
#else
#include <strings.h>
#define VFS_STRNICMP strncasecmp
#define VFS_STRICMP strcasecmp
#endif

std::string VirtualFileSystem::NormalizeVirtualPath(std::string_view path)
{
    std::string normalized;
    normalized.reserve(path.size() + 1);

    for (char c : path)
    {
        normalized.push_back(c == '\\' ? '/' : c);
    }

    std::vector<std::string_view> segments;
    size_t start = 0;
    while (start < normalized.size())
    {
        while (start < normalized.size() && normalized[start] == '/')
        {
            ++start;
        }
        if (start >= normalized.size()) break;

        size_t end = normalized.find('/', start);
        if (end == std::string_view::npos)
        {
            end = normalized.size();
        }

        std::string_view seg(normalized.data() + start, end - start);
        if (seg == ".")
        {
            // Current directory segment, skip
        }
        else if (seg == "..")
        {
            // Directory traversal prevention: pop previous segment if any
            if (!segments.empty())
            {
                segments.pop_back();
            }
        }
        else
        {
            segments.push_back(seg);
        }
        start = end;
    }

    if (segments.empty())
    {
        return "/";
    }

    std::string result;
    for (const auto& seg : segments)
    {
        result.push_back('/');
        result.append(seg.data(), seg.size());
    }
    return result;
}

void VirtualFileSystem::Mount(std::string_view virtualRoot, const std::filesystem::path& physicalPath)
{
    const std::string normRoot = NormalizeVirtualPath(virtualRoot);
    const auto absPhysical = std::filesystem::absolute(physicalPath).lexically_normal();

    std::unique_lock<std::shared_mutex> lock(mutex);
    auto it = std::find_if(mounts.begin(), mounts.end(), [&](const MountPoint& m) {
        return VFS_STRICMP(m.virtualRoot.c_str(), normRoot.c_str()) == 0;
    });

    if (it != mounts.end())
    {
        it->physicalPath = absPhysical;
    }
    else
    {
        mounts.push_back({ normRoot, absPhysical });
    }

    // Sort by length descending to ensure longest prefix match
    std::sort(mounts.begin(), mounts.end(), [](const MountPoint& a, const MountPoint& b) {
        return a.virtualRoot.size() > b.virtualRoot.size();
    });
}

bool VirtualFileSystem::Unmount(std::string_view virtualRoot)
{
    const std::string normRoot = NormalizeVirtualPath(virtualRoot);
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto it = std::find_if(mounts.begin(), mounts.end(), [&](const MountPoint& m) {
        return VFS_STRICMP(m.virtualRoot.c_str(), normRoot.c_str()) == 0;
    });

    if (it != mounts.end())
    {
        mounts.erase(it);
        return true;
    }
    return false;
}

void VirtualFileSystem::UnmountAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex);
    mounts.clear();
}

std::optional<std::filesystem::path> VirtualFileSystem::Resolve(std::string_view virtualPath) const
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);

    std::shared_lock<std::shared_mutex> lock(mutex);
    for (const auto& mount : mounts)
    {
        const size_t rootLen = mount.virtualRoot.size();
        if (mount.virtualRoot == "/")
        {
            const std::string subPath = normPath == "/" ? "" : normPath.substr(1);
            return (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
        }

        if (normPath.size() == rootLen && VFS_STRICMP(normPath.c_str(), mount.virtualRoot.c_str()) == 0)
        {
            return mount.physicalPath;
        }

        if (normPath.size() > rootLen &&
            normPath[rootLen] == '/' &&
            VFS_STRNICMP(normPath.c_str(), mount.virtualRoot.c_str(), rootLen) == 0)
        {
            const std::string subPath = normPath.substr(rootLen + 1);
            return (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
        }
    }

    return std::nullopt;
}

bool VirtualFileSystem::Exists(std::string_view virtualPath) const
{
    const auto resolved = Resolve(virtualPath);
    if (!resolved)
    {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(*resolved, ec);
}

std::vector<uint8_t> VirtualFileSystem::ReadBytes(std::string_view virtualPath) const
{
    const auto resolved = Resolve(virtualPath);
    if (!resolved)
    {
        throw std::runtime_error("Virtual file not found (unmounted root): " + std::string(virtualPath));
    }

    std::ifstream stream(resolved->c_str(), std::ios::binary | std::ios::ate);
    if (!stream.is_open())
    {
        throw std::runtime_error("Failed to open virtual file: " + std::string(virtualPath) + " -> " + resolved->generic_u8string());
    }

    const auto fileSize = stream.tellg();
    if (fileSize < 0)
    {
        throw std::runtime_error("Failed to query file size: " + std::string(virtualPath));
    }

    if (fileSize == 0)
    {
        return {};
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        throw std::runtime_error("Failed to read virtual file bytes: " + std::string(virtualPath));
    }

    return buffer;
}

std::string VirtualFileSystem::ReadText(std::string_view virtualPath) const
{
    auto bytes = ReadBytes(virtualPath);
    if (bytes.empty())
    {
        return {};
    }

    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        offset = 3;
    }

    return std::string(reinterpret_cast<const char*>(bytes.data() + offset), bytes.size() - offset);
}

std::vector<VirtualFileSystem::MountPoint> VirtualFileSystem::GetMounts() const
{
    std::shared_lock<std::shared_mutex> lock(mutex);
    return mounts;
}
