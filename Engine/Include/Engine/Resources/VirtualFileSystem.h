#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

class VirtualFileSystem
{
public:
    struct MountPoint
    {
        std::string virtualRoot;            // E.g., "/Engine", "/Game"
        std::filesystem::path physicalPath; // Absolute normalized filesystem path
    };

    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
    VirtualFileSystem(VirtualFileSystem&&) noexcept = default;
    VirtualFileSystem& operator=(VirtualFileSystem&&) noexcept = default;

    // Mounts a physical directory to a virtual root (e.g. "/Engine" -> "C:/.../Engine/Content").
    // If virtualRoot is already mounted, its physical path is updated.
    void Mount(std::string_view virtualRoot, const std::filesystem::path& physicalPath);

    // Unmounts a virtual root. Returns true if found and removed.
    bool Unmount(std::string_view virtualRoot);

    // Removes all mount points.
    void UnmountAll();

    // Resolves a virtual path (e.g. "/Engine/Shaders/Shadows.hlsl") to a physical filesystem path.
    // Returns std::nullopt if no mount point matches the virtual path.
    std::optional<std::filesystem::path> Resolve(std::string_view virtualPath) const;

    // Checks if the virtual path resolves to an existing file or directory.
    bool Exists(std::string_view virtualPath) const;

    // Safe RAII binary file reader. Throws std::runtime_error on failure.
    std::vector<uint8_t> ReadBytes(std::string_view virtualPath) const;

    // Safe RAII text file reader. Strips UTF-8 BOM if present. Throws std::runtime_error on failure.
    std::string ReadText(std::string_view virtualPath) const;

    // Returns a copy of all current mount points.
    std::vector<MountPoint> GetMounts() const;

    // Normalizes a virtual path: replaces '\' with '/', resolves '.' and '..', deduplicates slashes.
    static std::string NormalizeVirtualPath(std::string_view path);

private:
    mutable std::shared_mutex mutex;
    std::vector<MountPoint> mounts;
};
