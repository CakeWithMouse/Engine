#define NOMINMAX
#include <Engine/Base/Base.h>
#include <Engine/Resources/Paths.h>
#include "Project.h"
#include "ProjectUpgrade.h"
#include "PreviewScenes.h"
#include <cstdio>
#include <set>
#include <stdexcept>

int wmain(int argc, wchar_t** argv)
{
    // Keep lifecycle/viewport diagnostics visible when launched with redirected output.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try
    {
        if (argc == 5 && std::wstring(argv[1]) == L"--upgrade-project" && std::wstring(argv[3]) == L"--destination")
        {
            ProjectUpgrade::Copy(argv[2],argv[4],EnginePaths::ExecutableDirectory());
            std::puts("Project upgraded into a new copy. Original preserved.");
            return 0;
        }
        RuntimeLaunchOptions options;
        std::filesystem::path projectPath;
        std::set<std::wstring> seen;
        bool sceneApi = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring key = argv[i];
            if (!seen.insert(key).second) throw std::runtime_error("Duplicate launch option");
            if (key == L"--test-fail-engine") { options.failEngineForTest = true; continue; }
            if (key == L"--scene-api") { sceneApi = true; continue; }
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
        if (!project.engineVersion.empty())
        {
            const auto release = ReadEngineRelease(directory);
            if (project.engineVersion != release.version || project.engineConfiguration != release.configuration)
                throw std::runtime_error("Project is pinned to another engine release. Open it through EngineLauncher.");
        }
        options.projectName = project.name;
        options.projectFile = project.file.u8string();
        options.contentRoot = project.content.u8string();
        options.module = options.mode == RuntimeMode::Game ? project.module : directory / "Editor.dll";
        if (options.mode == RuntimeMode::Game) options.startupScene = project.startupScene;
        if (sceneApi || (options.mode == RuntimeMode::Editor && !project.startupScene.empty() && project.preview.empty()))
        {
            if (options.mode != RuntimeMode::Editor) throw std::runtime_error("--scene-api requires editor mode");
            options.editModule = project.module;
            options.startupScene = project.startupScene;
        }
        EnginePaths::SetContentRoot(directory / "EngineContent");
        std::printf("Project: %s\nMode: %s\n", options.projectName.c_str(), options.mode == RuntimeMode::Game ? "game" : "editor");
        BaseEngine engine;
        if (options.mode == RuntimeMode::Editor)
        {
            engine.SetViewportFactory(FindPreviewScene(project.preview, project.content));
            std::printf("PreviewScene: %s\n", project.preview.empty() ? "none" : project.preview.c_str());
        }
        return engine.Run(options);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "LaunchError: %s\n", error.what());
        return 1;
    }
}
