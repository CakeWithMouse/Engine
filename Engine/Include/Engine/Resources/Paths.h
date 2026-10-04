#pragma once
#include <filesystem>

namespace EnginePaths
{
    std::filesystem::path ExecutableDirectory();
    // Configure before initializing a Game. Absolute or resolved against the caller's CWD.
    void SetContentRoot(const std::filesystem::path& root);
    std::filesystem::path ContentRoot();
    std::filesystem::path Shader(const char* name);
}
