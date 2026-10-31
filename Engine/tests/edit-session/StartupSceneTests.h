#include "../../Runtime/Project.h"

static void ExerciseStartup(const std::filesystem::path& module, const std::filesystem::path& consumer)
{
    const auto folder = std::filesystem::temp_directory_path();
    const auto stem = L"engine-startup-" + std::to_wstring(GetCurrentProcessId());
    const auto projectFile = folder / (stem + L".project.json");
    const auto sceneFile = folder / (stem + L"-\u0441\u0446\u0435\u043d\u0430.json");
    const auto marker = std::filesystem::path(projectFile.wstring() + L".consumer-created");
    struct Cleanup {
        std::filesystem::path a, b, c;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(a, ec); std::filesystem::remove(b, ec); std::filesystem::remove(c, ec); }
    } cleanup{projectFile, sceneFile, marker};
    auto write = [](const std::filesystem::path& path, const std::string& text) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc); file << text; file.close();
        Check(bool(file), "startup fixture written");
    };
    nlohmann::json json = {{"name", "startup-scene-test"}, {"module", module.u8string()}, {"content", "."}};
    write(projectFile, json.dump());
    Check(ReadRuntimeProject(projectFile).startupScene.empty(), "startupScene optional");
    json["startupScene"] = sceneFile.filename().u8string();
    json["preview"] = "legacy-unused";
    write(projectFile, json.dump());
    const auto project = ReadRuntimeProject(projectFile);
    Check(project.startupScene == sceneFile && project.preview == "legacy-unused", "startup resolves beside project; preview retained");
    // Resolution must not depend on the launcher's working directory.
    const auto oldCwd = std::filesystem::current_path();
    struct RestoreCwd { std::filesystem::path path; ~RestoreCwd() { std::error_code ec; std::filesystem::current_path(path, ec); } } restoreCwd{oldCwd};
    std::filesystem::current_path(module.parent_path());
    Check(ReadRuntimeProject(projectFile).startupScene == sceneFile, "startup independent of cwd");
    std::filesystem::current_path(oldCwd);
    for (auto value : {nlohmann::json(""), nlohmann::json(42), nlohmann::json(nullptr),
        nlohmann::json("C:/absolute.json"), nlohmann::json("C:relative.json"), nlohmann::json("/rooted.json"),
        nlohmann::json(std::string("a\0b", 3))})
    {
        auto bad = json; bad["startupScene"] = value; write(projectFile, bad.dump());
        bool rejected = false;
        try { ReadRuntimeProject(projectFile); } catch (const std::exception&) { rejected = true; }
        Check(rejected, "invalid startup project field");
    }
    write(projectFile, json.dump());
    SceneSnapshot data; data.name = "Startup";
    SceneComponentSnapshot object; object.id = 10; object.name = "Startup object";
    object.typeName = "RuntimeExample::Mover"; object.transform.position.x = 5;
    data.components.push_back(object);
    SceneDocument::Write(sceneFile, data);
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Editor; options.module = consumer; options.editModule = module;
    options.startupScene = project.startupScene; options.projectName = project.name;
    options.projectFile = project.file.u8string(); options.contentRoot = project.content.u8string(); options.frames = 3;
    auto run = [&] { BaseEngine engine; return engine.Run(options); };
    std::filesystem::remove(marker);
    Check(run() == 0 && std::filesystem::exists(marker), "startup loaded before bind and static through Play/Stop");
    auto reject = [&] {
        std::filesystem::remove(marker);
        Check(run() != 0 && !std::filesystem::exists(marker), "startup failure prevents consumer creation");
    };
    data.components[0].typeName = "Missing::Class"; SceneDocument::Write(sceneFile, data); reject();
    write(sceneFile, "{"); reject();
    write(sceneFile, "{\"version\":2}"); reject();
    std::filesystem::remove(sceneFile);
    Check(ReadRuntimeProject(projectFile).startupScene == sceneFile, "project parse does not open scene file");
    reject();
    // Game mode now requires a valid startup scene too.
    options.mode = RuntimeMode::Game; options.module = module; options.editModule.clear(); options.frames = 1;
    Check(run() != 0, "game mode rejects missing startupScene");
    // Recover with another clean run after all failed starts.
    data.components[0].typeName = "RuntimeExample::Mover"; SceneDocument::Write(sceneFile, data);
    Check(run() == 0, "game mode loads startupScene");
    options.startupScene.clear();
    Check(run() == 0, "legacy game without startupScene");
    options.startupScene = project.startupScene;
    options.mode = RuntimeMode::Editor; options.module = consumer; options.editModule = module; options.frames = 3;
    Check(run() == 0, "startup recovers after failed starts");
}
