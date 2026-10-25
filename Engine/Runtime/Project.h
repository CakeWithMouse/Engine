#pragma once
#include <filesystem>
#include <string>

struct RuntimeProject
{
    std::string name;
    std::filesystem::path file, module, content;
    std::string preview;
    std::filesystem::path startupScene; // absolute resolved path; empty when omitted
    std::string engineVersion, engineConfiguration, templateId;
};

// Required name/module/content; optional preview and project-relative startupScene.
RuntimeProject ReadRuntimeProject(const std::filesystem::path& file);

struct EngineRelease
{
    std::string version, configuration;
    std::filesystem::path runtime;
};
EngineRelease ReadEngineRelease(const std::filesystem::path& runtime);
std::filesystem::path EngineStore();
EngineRelease InstallEngineRelease(const std::filesystem::path& runtime);
EngineRelease ResolveEngineRelease(const RuntimeProject&, const std::filesystem::path& currentRuntime);
