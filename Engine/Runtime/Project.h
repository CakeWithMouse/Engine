#pragma once
#include <filesystem>
#include <string>

struct RuntimeProject
{
    std::string name;
    std::filesystem::path file, module, content;
    std::string preview;
};

// Three required string fields, plus an optional built-in preview scene identifier.
RuntimeProject ReadRuntimeProject(const std::filesystem::path& file);
