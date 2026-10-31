#pragma once
#include <filesystem>
#include <functional>
#include <string>
namespace AssetUpdate {
using Commit = std::function<void(const std::string&,const std::string&)>;
// Files are restored if validation/live-scene replacement throws. Successful
// edits retain the original bytes in project/.painengine/AssetBackups.
void Run(const std::filesystem::path& content,unsigned operation,const std::string& path,
         const std::string& argument,const Commit& commit);
std::string Remap(const std::string& value,const std::string& before,const std::string& after);
}
