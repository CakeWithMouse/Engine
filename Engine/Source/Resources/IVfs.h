#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class IVfs
{
public:
    virtual ~IVfs() = default;

    virtual bool Exists(std::string_view virtualPath) const = 0;
    virtual std::vector<uint8_t> ReadBytes(std::string_view path, std::string* outVirtualPath = nullptr) const = 0;
    virtual std::string ToVirtualPath(std::string_view path) const = 0;
};
