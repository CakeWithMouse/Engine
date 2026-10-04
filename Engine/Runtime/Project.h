#pragma once
#include <filesystem>
#include <string>

struct RuntimeProject
{
    std::string name;
    std::filesystem::path file, module, content;
};

// Deliberately narrow schema: JSON object with three required string fields.
RuntimeProject ReadRuntimeProject(const std::filesystem::path& file);
