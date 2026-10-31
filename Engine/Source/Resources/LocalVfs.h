#pragma once

#include "IVfs.h"
#include <Engine/Resources/VirtualFileSystem.h>

class LocalVfs : public IVfs
{
public:
    static LocalVfs& Instance();

    bool Exists(std::string_view virtualPath) const override;
    std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr) const override;
    std::string ToVirtualPath(std::string_view path) const override;
    std::string Resolve(std::string_view virtualPath) const override;

    VirtualFileSystem& GetUnderlyingVfs();

private:
    LocalVfs() = default;
};
