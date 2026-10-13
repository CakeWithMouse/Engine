#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

enum class AssetType
{
    Unknown,
    Model,
    Texture,
    Shader,
    Script,
    Audio,
    Config
};

struct VirtualFileInfo
{
    std::string virtualPath;
    std::filesystem::path physicalPath;
    AssetType type = AssetType::Unknown;
    uintmax_t sizeBytes = 0;
    std::string extension;
};

const char* AssetTypeToString(AssetType type);
AssetType StringToAssetType(std::string_view str);
AssetType DeduceAssetType(std::string_view pathOrExtension);

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

    // Determines the asset type of a virtual path by its extension.
    AssetType GetAssetType(std::string_view virtualPath) const;

    // Queries file metadata for a virtual path. Returns std::nullopt if the file does not exist.
    std::optional<VirtualFileInfo> GetFileInfo(std::string_view virtualPath) const;

    // Enumerates files inside a virtual directory.
    std::vector<VirtualFileInfo> EnumerateFiles(std::string_view virtualDir, bool recursive = true) const;

    // Searches for files inside a virtual directory with optional AssetType and wildcard/substring pattern filters.
    std::vector<VirtualFileInfo> FindFiles(std::string_view virtualDir, AssetType filterType, std::string_view namePattern = "", bool recursive = true) const;

    // Copies an external file into a target virtual directory and returns its file info.
    std::optional<VirtualFileInfo> ImportFile(const std::filesystem::path& externalPhysicalPath, std::string_view targetVirtualDir);

    // Recursively creates a directory at the given virtual path.
    bool CreateDir(std::string_view virtualDir);

#ifdef DeleteFile
#undef DeleteFile
#endif
    // Safely deletes a file at the given virtual path. Protected roots cannot be deleted.
    bool DeleteFile(std::string_view virtualPath);
#if defined(_WIN32)
    bool DeleteFileW(std::string_view virtualPath) { return DeleteFile(virtualPath); }
#endif

    // Deletes a directory at the given virtual path. Protected roots cannot be deleted.
    bool DeleteDir(std::string_view virtualDir, bool recursive = true);

    // Moves or renames a file or directory between virtual paths.
    bool Move(std::string_view sourceVirtualPath, std::string_view targetVirtualPath);

    // Copies a file or directory between virtual paths.
    bool Copy(std::string_view sourceVirtualPath, std::string_view targetVirtualPath, bool overwrite = true);

    // Safely writes raw bytes to a file via VFS. Creates parent directories if needed.
    bool WriteBytes(std::string_view virtualPath, const std::vector<uint8_t>& bytes);

    // Safely writes text to a file via VFS. Creates parent directories if needed.
    bool WriteText(std::string_view virtualPath, std::string_view text);

private:
    std::optional<std::filesystem::path> ResolveInternal(std::string_view normPath) const;
    bool IsProtectedRoot(std::string_view normPath) const;

    mutable std::shared_mutex mutex;
    std::vector<MountPoint> mounts;
};
