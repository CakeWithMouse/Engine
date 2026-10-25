#pragma once
#include <filesystem>
#include <string>

struct RuntimeProject
{
    std::string name;
    std::filesystem::path file, module, content;
    std::string preview;
    std::filesystem::path startupScene; // absolute resolved path; empty when omitted
};

// Required name/module/content; optional preview and project-relative startupScene.
RuntimeProject ReadRuntimeProject(const std::filesystem::path& file);
