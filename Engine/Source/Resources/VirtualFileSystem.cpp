#include <Engine/Resources/VirtualFileSystem.h>

#include <algorithm>
#include <cctype>
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

namespace
{
bool WildcardMatch(std::string_view pattern, std::string_view text)
{
    size_t p = 0, t = 0;
    size_t starP = std::string_view::npos, starT = 0;

    auto toLower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    while (t < text.size())
    {
        if (p < pattern.size() && (pattern[p] == '?' || toLower(pattern[p]) == toLower(text[t])))
        {
            ++p;
            ++t;
        }
        else if (p < pattern.size() && pattern[p] == '*')
        {
            starP = p++;
            starT = t;
        }
        else if (starP != std::string_view::npos)
        {
            p = starP + 1;
            t = ++starT;
        }
        else
        {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*')
    {
        ++p;
    }

    return p == pattern.size();
}

bool CaseInsensitiveContains(std::string_view text, std::string_view sub)
{
    if (sub.empty()) return true;
    if (sub.size() > text.size()) return false;

    auto toLower = [](char c) -> char {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };

    for (size_t i = 0; i <= text.size() - sub.size(); ++i)
    {
        bool match = true;
        for (size_t j = 0; j < sub.size(); ++j)
        {
            if (toLower(text[i + j]) != toLower(sub[j]))
            {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}
}

const char* AssetTypeToString(AssetType type)
{
    switch (type)
    {
    case AssetType::Model:   return "Model";
    case AssetType::Texture: return "Texture";
    case AssetType::Shader:  return "Shader";
    case AssetType::Script:  return "Script";
    case AssetType::Audio:   return "Audio";
    case AssetType::Config:  return "Config";
    case AssetType::Unknown:
    default:                 return "Unknown";
    }
}

AssetType StringToAssetType(std::string_view str)
{
    if (CaseInsensitiveContains(str, "model")) return AssetType::Model;
    if (CaseInsensitiveContains(str, "texture")) return AssetType::Texture;
    if (CaseInsensitiveContains(str, "shader")) return AssetType::Shader;
    if (CaseInsensitiveContains(str, "script")) return AssetType::Script;
    if (CaseInsensitiveContains(str, "audio")) return AssetType::Audio;
    if (CaseInsensitiveContains(str, "config")) return AssetType::Config;
    return AssetType::Unknown;
}

AssetType DeduceAssetType(std::string_view pathOrExtension)
{
    if (pathOrExtension.empty()) return AssetType::Unknown;

    const size_t lastSlash = pathOrExtension.find_last_of("/\\");
    const size_t lastDot = pathOrExtension.find_last_of('.');

    std::string ext;
    if (lastDot != std::string_view::npos && (lastSlash == std::string_view::npos || lastDot > lastSlash))
    {
        ext = std::string(pathOrExtension.substr(lastDot));
    }
    else if (lastSlash == std::string_view::npos)
    {
        ext = "." + std::string(pathOrExtension);
    }
    else
    {
        return AssetType::Unknown;
    }

    for (char& c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".blend" || ext == ".dae")
        return AssetType::Model;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp" || ext == ".hdr")
        return AssetType::Texture;
    if (ext == ".hlsl" || ext == ".fx" || ext == ".glsl" || ext == ".cso")
        return AssetType::Shader;
    if (ext == ".lua")
        return AssetType::Script;
    if (ext == ".json" || ext == ".ini" || ext == ".xml" || ext == ".txt")
        return AssetType::Config;
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
        return AssetType::Audio;

    return AssetType::Unknown;
}

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

bool VirtualFileSystem::IsProtectedRoot(std::string_view normPath) const
{
    if (normPath == "/" || VFS_STRICMP(std::string(normPath).c_str(), "/Engine") == 0)
    {
        return true;
    }
    for (const auto& mount : mounts)
    {
        if (VFS_STRICMP(std::string(normPath).c_str(), mount.virtualRoot.c_str()) == 0)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::filesystem::path> VirtualFileSystem::ResolveInternal(std::string_view normPath) const
{
    for (const auto& mount : mounts)
    {
        const size_t rootLen = mount.virtualRoot.size();
        if (mount.virtualRoot == "/")
        {
            const std::string subPath = normPath == "/" ? "" : std::string(normPath.substr(1));
            return (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
        }

        if (normPath.size() == rootLen && VFS_STRICMP(std::string(normPath).c_str(), mount.virtualRoot.c_str()) == 0)
        {
            return mount.physicalPath;
        }

        if (normPath.size() > rootLen &&
            normPath[rootLen] == '/' &&
            VFS_STRNICMP(std::string(normPath).c_str(), mount.virtualRoot.c_str(), rootLen) == 0)
        {
            const std::string subPath = std::string(normPath.substr(rootLen + 1));
            return (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> VirtualFileSystem::Resolve(std::string_view virtualPath) const
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);
    std::shared_lock<std::shared_mutex> lock(mutex);
    return ResolveInternal(normPath);
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

AssetType VirtualFileSystem::GetAssetType(std::string_view virtualPath) const
{
    return DeduceAssetType(virtualPath);
}

std::optional<VirtualFileInfo> VirtualFileSystem::GetFileInfo(std::string_view virtualPath) const
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);
    const auto resolved = Resolve(normPath);
    if (!resolved)
    {
        return std::nullopt;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*resolved, ec) || !std::filesystem::is_regular_file(*resolved, ec))
    {
        return std::nullopt;
    }

    const auto size = std::filesystem::file_size(*resolved, ec);
    const uintmax_t sizeBytes = ec ? 0 : size;
    std::string ext = resolved->extension().generic_u8string();
    const AssetType type = DeduceAssetType(ext);

    return VirtualFileInfo{ normPath, *resolved, type, sizeBytes, std::move(ext) };
}

std::vector<VirtualFileInfo> VirtualFileSystem::EnumerateFiles(std::string_view virtualDir, bool recursive) const
{
    const std::string normDir = NormalizeVirtualPath(virtualDir);
    std::vector<VirtualFileInfo> results;

    std::shared_lock<std::shared_mutex> lock(mutex);

    auto isHidden = [](const std::filesystem::path& p) {
        const auto filename = p.filename().generic_u8string();
        return !filename.empty() && filename[0] == '.';
    };

    auto processFile = [&](const std::filesystem::path& physPath, const std::string& vPath) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(physPath, ec) || isHidden(physPath)) return;
        const auto size = std::filesystem::file_size(physPath, ec);
        const uintmax_t sizeBytes = ec ? 0 : size;
        std::string ext = physPath.extension().generic_u8string();
        const AssetType type = DeduceAssetType(ext);
        results.push_back(VirtualFileInfo{ vPath, physPath, type, sizeBytes, std::move(ext) });
    };

    for (const auto& mount : mounts)
    {
        std::error_code ec;
        const size_t rootLen = mount.virtualRoot.size();

        // Case 1: normDir is within or equal to mount.virtualRoot
        if (mount.virtualRoot == "/" ||
            (normDir.size() == rootLen && VFS_STRICMP(normDir.c_str(), mount.virtualRoot.c_str()) == 0) ||
            (normDir.size() > rootLen && normDir[rootLen] == '/' && VFS_STRNICMP(normDir.c_str(), mount.virtualRoot.c_str(), rootLen) == 0))
        {
            std::filesystem::path targetPhys;
            if (mount.virtualRoot == "/")
            {
                const std::string subPath = normDir == "/" ? "" : normDir.substr(1);
                targetPhys = (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
            }
            else if (normDir.size() == rootLen)
            {
                targetPhys = mount.physicalPath;
            }
            else
            {
                const std::string subPath = normDir.substr(rootLen + 1);
                targetPhys = (mount.physicalPath / std::filesystem::u8path(subPath)).lexically_normal();
            }

            if (!std::filesystem::exists(targetPhys, ec) || !std::filesystem::is_directory(targetPhys, ec))
            {
                continue;
            }

            if (recursive)
            {
                for (std::filesystem::recursive_directory_iterator it(targetPhys, std::filesystem::directory_options::skip_permission_denied, ec), end;
                     it != end && !ec;
                     it.increment(ec))
                {
                    if (ec) { ec.clear(); continue; }
                    if (it->is_directory(ec))
                    {
                        if (isHidden(it->path())) it.disable_recursion_pending();
                        continue;
                    }
                    const auto rel = std::filesystem::relative(it->path(), mount.physicalPath, ec);
                    if (ec) continue;
                    std::string vPath = NormalizeVirtualPath(mount.virtualRoot + "/" + rel.generic_u8string());
                    processFile(it->path(), vPath);
                }
            }
            else
            {
                for (std::filesystem::directory_iterator it(targetPhys, std::filesystem::directory_options::skip_permission_denied, ec), end;
                     it != end && !ec;
                     it.increment(ec))
                {
                    if (ec) { ec.clear(); continue; }
                    if (it->is_directory(ec)) continue;
                    const auto rel = std::filesystem::relative(it->path(), mount.physicalPath, ec);
                    if (ec) continue;
                    std::string vPath = NormalizeVirtualPath(mount.virtualRoot + "/" + rel.generic_u8string());
                    processFile(it->path(), vPath);
                }
            }
        }
        // Case 2: mount.virtualRoot is inside normDir (e.g. normDir is "/", mount is "/Engine")
        else if (recursive &&
                 (normDir == "/" ||
                  (mount.virtualRoot.size() > normDir.size() &&
                   mount.virtualRoot[normDir.size()] == '/' &&
                   VFS_STRNICMP(mount.virtualRoot.c_str(), normDir.c_str(), normDir.size()) == 0)))
        {
            if (!std::filesystem::exists(mount.physicalPath, ec) || !std::filesystem::is_directory(mount.physicalPath, ec))
            {
                continue;
            }

            for (std::filesystem::recursive_directory_iterator it(mount.physicalPath, std::filesystem::directory_options::skip_permission_denied, ec), end;
                 it != end && !ec;
                 it.increment(ec))
                {
                if (ec) { ec.clear(); continue; }
                if (it->is_directory(ec))
                {
                    if (isHidden(it->path())) it.disable_recursion_pending();
                    continue;
                }
                const auto rel = std::filesystem::relative(it->path(), mount.physicalPath, ec);
                if (ec) continue;
                std::string vPath = NormalizeVirtualPath(mount.virtualRoot + "/" + rel.generic_u8string());
                processFile(it->path(), vPath);
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const VirtualFileInfo& a, const VirtualFileInfo& b) {
        return VFS_STRICMP(a.virtualPath.c_str(), b.virtualPath.c_str()) < 0;
    });

    results.erase(std::unique(results.begin(), results.end(), [](const VirtualFileInfo& a, const VirtualFileInfo& b) {
        return VFS_STRICMP(a.virtualPath.c_str(), b.virtualPath.c_str()) == 0;
    }), results.end());

    return results;
}

std::vector<VirtualFileInfo> VirtualFileSystem::FindFiles(std::string_view virtualDir, AssetType filterType, std::string_view namePattern, bool recursive) const
{
    const auto allFiles = EnumerateFiles(virtualDir, recursive);
    std::vector<VirtualFileInfo> matches;
    matches.reserve(allFiles.size());

    for (const auto& file : allFiles)
    {
        if (filterType != AssetType::Unknown)
        {
            if (file.type != filterType) continue;
        }
        else if (namePattern.empty())
        {
            if (file.type != AssetType::Unknown) continue;
        }

        if (!namePattern.empty())
        {
            const std::string filename = std::filesystem::path(file.virtualPath).filename().generic_u8string();
            const bool wildcardMatched = WildcardMatch(namePattern, filename);
            const bool substringMatched = CaseInsensitiveContains(filename, namePattern) ||
                                          CaseInsensitiveContains(file.virtualPath, namePattern);

            if (!wildcardMatched && !substringMatched)
            {
                continue;
            }
        }

        matches.push_back(file);
    }

    return matches;
}

std::optional<VirtualFileInfo> VirtualFileSystem::ImportFile(const std::filesystem::path& externalPhysicalPath, std::string_view targetVirtualDir)
{
    std::error_code ec;
    if (!std::filesystem::exists(externalPhysicalPath, ec) || !std::filesystem::is_regular_file(externalPhysicalPath, ec))
    {
        return std::nullopt;
    }

    const std::string normDir = NormalizeVirtualPath(targetVirtualDir);
    const auto resolvedDir = Resolve(normDir);
    if (!resolvedDir)
    {
        return std::nullopt;
    }

    std::filesystem::create_directories(*resolvedDir, ec);
    if (ec)
    {
        return std::nullopt;
    }

    const auto filename = externalPhysicalPath.filename();
    if (filename.empty())
    {
        return std::nullopt;
    }

    const auto targetPhysicalPath = *resolvedDir / filename;
    std::filesystem::copy_file(externalPhysicalPath, targetPhysicalPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        return std::nullopt;
    }

    std::string targetVirtualPath;
    if (normDir == "/")
    {
        targetVirtualPath = "/" + filename.generic_u8string();
    }
    else
    {
        targetVirtualPath = normDir + "/" + filename.generic_u8string();
    }

    return GetFileInfo(targetVirtualPath);
}

bool VirtualFileSystem::CreateDir(std::string_view virtualDir)
{
    const std::string normDir = NormalizeVirtualPath(virtualDir);
    if (normDir == "/")
    {
        return true;
    }

    std::unique_lock<std::shared_mutex> lock(mutex);
    const auto resolved = ResolveInternal(normDir);
    if (!resolved)
    {
        return false;
    }

    std::error_code ec;
    return std::filesystem::create_directories(*resolved, ec) || std::filesystem::is_directory(*resolved, ec);
}

bool VirtualFileSystem::DeleteFile(std::string_view virtualPath)
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (IsProtectedRoot(normPath))
    {
        return false;
    }

    const auto resolved = ResolveInternal(normPath);
    if (!resolved)
    {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*resolved, ec) || std::filesystem::is_directory(*resolved, ec))
    {
        return false;
    }

    return std::filesystem::remove(*resolved, ec) && !ec;
}

bool VirtualFileSystem::DeleteDir(std::string_view virtualDir, bool recursive)
{
    const std::string normDir = NormalizeVirtualPath(virtualDir);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (IsProtectedRoot(normDir))
    {
        return false;
    }

    const auto resolved = ResolveInternal(normDir);
    if (!resolved)
    {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*resolved, ec) || !std::filesystem::is_directory(*resolved, ec))
    {
        return false;
    }

    if (recursive)
    {
        const auto count = std::filesystem::remove_all(*resolved, ec);
        return !ec && count > 0;
    }
    else
    {
        return std::filesystem::remove(*resolved, ec) && !ec;
    }
}

bool VirtualFileSystem::Move(std::string_view sourceVirtualPath, std::string_view targetVirtualPath)
{
    const std::string normSrc = NormalizeVirtualPath(sourceVirtualPath);
    const std::string normDst = NormalizeVirtualPath(targetVirtualPath);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (IsProtectedRoot(normSrc) || IsProtectedRoot(normDst))
    {
        return false;
    }

    if (VFS_STRICMP(normSrc.c_str(), normDst.c_str()) == 0)
    {
        return true;
    }

    const auto srcResolved = ResolveInternal(normSrc);
    const auto dstResolved = ResolveInternal(normDst);
    if (!srcResolved || !dstResolved)
    {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*srcResolved, ec))
    {
        return false;
    }

    auto finalDst = *dstResolved;
    if (std::filesystem::is_directory(*dstResolved, ec))
    {
        finalDst = *dstResolved / srcResolved->filename();
    }

    std::filesystem::create_directories(finalDst.parent_path(), ec);
    if (ec)
    {
        return false;
    }

    if (std::filesystem::exists(finalDst, ec))
    {
        std::filesystem::remove_all(finalDst, ec);
    }

    std::filesystem::rename(*srcResolved, finalDst, ec);
    if (ec)
    {
        ec.clear();
        std::filesystem::copy(*srcResolved, finalDst,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec)
        {
            std::filesystem::remove_all(*srcResolved, ec);
            return !ec;
        }
        return false;
    }

    return true;
}

bool VirtualFileSystem::Copy(std::string_view sourceVirtualPath, std::string_view targetVirtualPath, bool overwrite)
{
    const std::string normSrc = NormalizeVirtualPath(sourceVirtualPath);
    const std::string normDst = NormalizeVirtualPath(targetVirtualPath);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (normSrc == "/" || IsProtectedRoot(normDst))
    {
        return false;
    }

    const auto srcResolved = ResolveInternal(normSrc);
    const auto dstResolved = ResolveInternal(normDst);
    if (!srcResolved || !dstResolved)
    {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(*srcResolved, ec))
    {
        return false;
    }

    auto finalDst = *dstResolved;
    if (std::filesystem::is_directory(*dstResolved, ec))
    {
        finalDst = *dstResolved / srcResolved->filename();
    }

    std::filesystem::create_directories(finalDst.parent_path(), ec);
    if (ec)
    {
        return false;
    }

    auto options = std::filesystem::copy_options::recursive;
    if (overwrite)
    {
        options |= std::filesystem::copy_options::overwrite_existing;
    }

    std::filesystem::copy(*srcResolved, finalDst, options, ec);
    return !ec;
}

bool VirtualFileSystem::WriteBytes(std::string_view virtualPath, const std::vector<uint8_t>& bytes)
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (IsProtectedRoot(normPath))
    {
        return false;
    }

    const auto resolved = ResolveInternal(normPath);
    if (!resolved)
    {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec)
    {
        return false;
    }

    std::ofstream stream(resolved->c_str(), std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        return false;
    }

    if (!bytes.empty())
    {
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    return stream.good();
}

bool VirtualFileSystem::WriteText(std::string_view virtualPath, std::string_view text)
{
    const std::string normPath = NormalizeVirtualPath(virtualPath);

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (IsProtectedRoot(normPath))
    {
        return false;
    }

    const auto resolved = ResolveInternal(normPath);
    if (!resolved)
    {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec)
    {
        return false;
    }

    std::ofstream stream(resolved->c_str(), std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        return false;
    }

    if (!text.empty())
    {
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    return stream.good();
}
