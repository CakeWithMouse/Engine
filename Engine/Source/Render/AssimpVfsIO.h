#pragma once
#if defined(ENGINE_ENABLE_ASSIMP)
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include "../Resources/VfsRead.h"
#include <cstring>
#include <algorithm>
#include <cctype>

class AssimpVfsStream final : public Assimp::IOStream
{
    std::vector<uint8_t> bytes_;
    size_t cursor_ = 0;
public:
    explicit AssimpVfsStream(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
    size_t Read(void* data, size_t size, size_t count) override
    {
        if (!size) return 0;
        count = (std::min)(count, (bytes_.size() - cursor_) / size);
        if (count) std::memcpy(data, bytes_.data() + cursor_, size * count);
        cursor_ += size * count; return count;
    }
    size_t Write(const void*, size_t, size_t) override { return 0; }
    aiReturn Seek(size_t offset, aiOrigin origin) override
    {
        if (origin == aiOrigin_END)
        { if (offset > bytes_.size()) return aiReturn_FAILURE; cursor_ = bytes_.size() - offset; }
        else
        {
            const size_t base = origin == aiOrigin_CUR ? cursor_ : 0;
            if (offset > bytes_.size() - base) return aiReturn_FAILURE;
            cursor_ = base + offset;
        }
        return aiReturn_SUCCESS;
    }
    size_t Tell() const override { return cursor_; }
    size_t FileSize() const override { return bytes_.size(); }
    void Flush() override {}
};
class AssimpVfsIO final : public Assimp::IOSystem
{
    std::string directory_;
    std::string file_;
    bool gltf_ = false;
    std::string Path(const char* value) const
    {
        std::string path = value ? value : "";
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.empty() || path.front() != '/') path = directory_ + path;
        if (gltf_ && path != file_)
        {
            auto hex = [](unsigned char c) -> int { if (c >= '0' && c <= '9') return c - '0'; c = static_cast<unsigned char>(std::tolower(c)); return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
            const size_t start = path.rfind(directory_, 0) == 0 ? directory_.size() : 0;
            std::string decoded = path.substr(0, start);
            for (size_t i = start; i < path.size(); ++i)
            {
                if (path[i] != '%') { decoded += path[i]; continue; }
                if (i + 2 >= path.size() || hex(path[i+1]) < 0 || hex(path[i+2]) < 0)
                    throw std::runtime_error("Invalid glTF URI");
                const char value = char(hex(path[i+1]) * 16 + hex(path[i+2]));
                if (!value) throw std::runtime_error("NUL in glTF URI");
                decoded += value; i += 2;
            }
            path = std::move(decoded);
        }
        return VirtualFileSystem::NormalizeVirtualPath(path);
    }
public:
    explicit AssimpVfsIO(std::string_view file) : directory_(file.substr(0, file.find_last_of('/') + 1)), file_(file)
    {
        auto extension = std::filesystem::u8path(file).extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        gltf_ = extension == ".gltf";
    }
    bool Exists(const char* file) const override
    { try { return VfsRead::Active().Exists(Path(file)); } catch (...) { return false; } }
    char getOsSeparator() const override { return '/'; }
    Assimp::IOStream* Open(const char* file, const char* mode = "rb") override
    {
        if (!mode || mode[0] != 'r') return nullptr;
        try
        {
            auto bytes = VfsRead::Active().ReadBytes(Path(file));
            return bytes.empty() ? nullptr : new AssimpVfsStream(std::move(bytes));
        }
        catch (...) { return nullptr; }
    }
    void Close(Assimp::IOStream* stream) override { delete stream; }
};
#endif
