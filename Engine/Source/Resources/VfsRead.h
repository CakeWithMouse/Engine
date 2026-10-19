#pragma once

#include <Engine/Resources/VirtualFileSystem.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace VfsRead
{
    std::filesystem::path FindRepoRoot(const std::filesystem::path& start = {});
    VirtualFileSystem& GetVFS();
    std::string ToVirtualPath(VirtualFileSystem& vfs, std::string_view path);
    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr);
}
