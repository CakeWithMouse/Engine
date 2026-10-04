#define NOMINMAX
#include <Engine/Base/Base.h>
#include <Engine/Resources/Paths.h>
#include "Project.h"
#include <cstdio>
#include <set>
#include <stdexcept>

int wmain(int argc, wchar_t** argv)
{
    try
    {
        RuntimeLaunchOptions options;
        std::filesystem::path projectPath;
        std::set<std::wstring> seen;
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring key = argv[i];
            if (!seen.insert(key).second) throw std::runtime_error("Duplicate launch option");
            if (key == L"--test-fail-engine") { options.failEngineForTest = true; continue; }
            if (++i == argc) throw std::runtime_error("Missing launch option value");
            const std::wstring value = argv[i];
            if (key == L"--project") projectPath = value;
            else if (key == L"--mode")
            {
                if (value != L"game" && value != L"editor") throw std::runtime_error("Mode must be game or editor");
                options.mode = value == L"game" ? RuntimeMode::Game : RuntimeMode::Editor;
            }
            else if (key == L"--frames")
            {
                if (value.empty() || value.find_first_not_of(L"0123456789") != std::wstring::npos)
                    throw std::runtime_error("Frame count must be a positive integer");
                options.frames = std::stoull(value);
                if (!options.frames) throw std::runtime_error("Frame count must be positive");
            }
            else throw std::runtime_error("Unknown launch option");
        }
        if (projectPath.empty() || !seen.count(L"--mode"))
            throw std::runtime_error("Usage: EngineRuntime --project Project.json --mode game|editor [--frames N]");
        const auto directory = EnginePaths::ExecutableDirectory();
        // Explicit relative project paths are relative to the host, never caller CWD.
        if (projectPath.is_relative()) projectPath = directory / projectPath;
        const auto project = ReadRuntimeProject(projectPath);
        options.projectName = project.name;
        options.projectFile = project.file.u8string();
        options.contentRoot = project.content.u8string();
        options.module = options.mode == RuntimeMode::Game ? project.module : directory / "Editor.dll";
        EnginePaths::SetContentRoot(directory / "EngineContent");
        std::printf("Project: %s\nMode: %s\n", options.projectName.c_str(), options.mode == RuntimeMode::Game ? "game" : "editor");
        BaseEngine engine;
        return engine.Run(options);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "LaunchError: %s\n", error.what());
        return 1;
    }
}
