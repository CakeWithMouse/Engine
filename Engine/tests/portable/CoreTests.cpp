#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/ModuleApi.h>
#include <Engine/Runtime/VfsApi.h>
#include <Engine/Scene/Trace.h>
#include <Engine/Script/ScriptRuntime.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("pain-portable-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{root};
    try
    {
        Check(fs::is_directory(EnginePaths::ExecutableDirectory()), "Executable directory");
        fs::create_directories(root / fs::u8path(u8"Тест с пробелами"));
        VirtualFileSystem vfs;
        vfs.Mount("/Game", root);
        Check(vfs.WriteText(u8"/Game/Тест с пробелами/example.txt", "hello"), "Write UTF-8 path");
        Check(vfs.ReadText(u8"/Game/Тест с пробелами/example.txt") == "hello", "Read UTF-8 path");
        Check(!vfs.Resolve("/Game/../../outside"), "Mount traversal must fail");
        Check(vfs.Move(u8"/Game/Тест с пробелами/example.txt", "/Game/renamed.txt"), "Move file");
        Check(vfs.ReadText("/Game/renamed.txt") == "hello", "Read moved file");
        EnginePaths::SetContentRoot(root);
        Check(EnginePaths::Shader("test.hlsl") == root / "Shaders/test.hlsl", "Content root");
        EnginePaths::SetContentRoot({});

        const auto ray = Trace::ScreenPointToRay({50, 50}, 100, 100, glm::mat4(1), glm::mat4(1));
        Check(bool(ray), "Unproject identity");
        Check(glm::length(ray.ray.origin) < 0.0001f, "Near plane origin");
        Check(glm::length(ray.ray.direction - glm::vec3(0, 0, 1)) < 0.0001f, "Ray direction");
        Check(Trace::ScreenPointToRay({0, 0}, 0, 100, glm::mat4(1), glm::mat4(1)).status == Trace::Status::InvalidViewport, "Zero viewport");
        Check(Trace::ScreenPointToRay({50, 50}, 100, 100, glm::mat4(0), glm::mat4(1)).status == Trace::Status::InvalidMatrix, "Singular matrix");
        Check(Trace::IntersectTriangle(ray.ray, {-1,-1,2}, {1,-1,2}, {0,1,2}).status == Trace::Status::Hit, "Triangle trace");
        const auto invalid = Script::Compile("not json");
        Check(!invalid.program && !invalid.problems.empty(), "Script diagnostics");
        Check(!Script::CatalogJson().empty() && Script::KeyCode("Space") != 0, "Script catalog");
        std::cout << "Portable core tests passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
