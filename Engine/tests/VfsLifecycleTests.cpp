#include "VfsLifecycleTests.h"

#include <Engine/Base/Base.h>
#include <Engine/Render/ShaderCompiler.h>
#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Runtime/LaunchOptions.h>
#include "../Source/Resources/VfsRead.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <wrl/client.h>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

namespace
{
std::filesystem::path FindFixtureDll()
{
    const auto execDir = EnginePaths::ExecutableDirectory();
    std::error_code ec;

    // 1. RuntimeFixtures/RequestExit.dll in current build dir
    const auto fixtureDll = execDir / "RuntimeFixtures" / "RequestExit.dll";
    if (std::filesystem::exists(fixtureDll, ec))
    {
        return fixtureDll.lexically_normal();
    }

    // 2. Game.dll in Game/build/x64/<Config>/Runtime/Projects/RuntimeExample/Game.dll
    const auto configName = execDir.filename().string();
    const auto repoRoot = VfsRead::FindRepoRoot(execDir);
    if (!repoRoot.empty())
    {
        const auto gameDll = repoRoot / "Game" / "build" / "x64" / configName / "Runtime" / "Projects" / "RuntimeExample" / "Game.dll";
        if (std::filesystem::exists(gameDll, ec))
        {
            return gameDll.lexically_normal();
        }
    }

    throw std::runtime_error("Fixture DLL not found for current configuration (checked: " +
        fixtureDll.string() + ")");
}
} // namespace

// Scenario A MUST run before any BaseEngine instance is created / run.
// It verifies that before any engine lifecycle events, VfsRead::ReadBytes
// successfully reads existing shader files via the untouched global VFS.
void VfsLifecycle_ScenarioA()
{
    const std::string shaderVirtualPath = "/Engine/Shaders/RotatedFigure.hlsl";
    const auto bytes = VfsRead::ReadBytes(shaderVirtualPath);

    std::cout << "[Scenario A] Global VFS initial mount check:\n"
              << "  File: " << shaderVirtualPath << "\n"
              << "  Bytes read: " << bytes.size() << "\n";

    CHECK(!bytes.empty());
}

// Scenario B: Run BaseEngine with frames=1 (Game mode) -> Stop.
// Then inspect VfsRead::GetVFS().GetMounts() and attempt VfsRead::ReadBytes.
void VfsLifecycle_ScenarioB()
{
    const auto fixtureDll = FindFixtureDll();
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Game;
    options.frames = 1;
    options.module = fixtureDll;

    std::cout << "[Scenario B] Running BaseEngine::Run with module: "
              << fixtureDll.string() << ", frames=1, mode=Game...\n";

    {
        BaseEngine engine;
        const int exitCode = engine.Run(options);
        std::cout << "[Scenario B] BaseEngine::Run finished with exitCode=" << exitCode << ".\n";
    }

    // Inspect VFS after Stop()
    const auto mounts = VfsRead::GetVFS().GetMounts();
    std::cout << "[Scenario B] VfsRead::GetVFS().GetMounts() after Stop(): count=" << mounts.size() << "\n";
    for (const auto& m : mounts)
    {
        std::cout << "    mount: '" << m.virtualRoot << "' -> '" << m.physicalPath.string() << "'\n";
    }

    const std::string shaderVirtualPath = "/Engine/Shaders/RotatedFigure.hlsl";
    const auto bytes = VfsRead::ReadBytes(shaderVirtualPath);
    std::cout << "[Scenario B] VfsRead::ReadBytes(\"" << shaderVirtualPath << "\") after Stop(): "
              << bytes.size() << " bytes.\n";

    // Architectural expectation: VFS consumer expects engine content to remain accessible.
    // If the hypothesis holds, BaseEngine::Stop() unmounted everything, so bytes is empty.
    CHECK(!bytes.empty());
}

// Scenario C: After Stop(), test TextureLoader and ShaderCompiler.
void VfsLifecycle_ScenarioC()
{
    // 1. TextureLoader: Requires active D3D11 device/context, skipped without simulating.
    std::cout << "[Scenario C] TextureLoader::LoadTexture2D: не проверено "
              << "(требует активный ID3D11Device* и ID3D11DeviceContext*, без D3D возвращает false до обращения к VFS).\n";

    // 2. ShaderCompiler::CompileFromFile on existing shader
    const std::wstring shaderVirtualPath = L"/Engine/Shaders/RotatedFigure.hlsl";
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    std::string compileError;

    const HRESULT hr = ShaderCompiler::CompileFromFile(
        shaderVirtualPath,
        nullptr,
        "VSMain",
        "vs_5_0",
        blob.GetAddressOf(),
        &compileError);

    std::cout << "[Scenario C] ShaderCompiler::CompileFromFile(\"/Engine/Shaders/RotatedFigure.hlsl\"):\n"
              << "  HRESULT: 0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n"
              << "  Error: " << (compileError.empty() ? "(none)" : compileError) << "\n";

    // Architectural expectation: Shader compiler should compile the existing shader.
    // If VFS has unmounted /Engine, CompileFromFile fails with ERROR_FILE_NOT_FOUND (0x80070002).
    CHECK(SUCCEEDED(hr));
    CHECK(blob != nullptr);
}

// Scenario D: Second Play after Stop(): new BaseEngine instance runs and exits cleanly.
void VfsLifecycle_ScenarioD()
{
    const auto fixtureDll = FindFixtureDll();
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Game;
    options.frames = 1;
    options.module = fixtureDll;

    std::cout << "[Scenario D] Второй Play после Stop (новый экземпляр BaseEngine)...\n";

    BaseEngine engine2;
    const int exitCode = engine2.Run(options);
    std::cout << "[Scenario D] BaseEngine::Run (второй экземпляр) вернул exitCode=" << exitCode << ".\n";
    CHECK(exitCode == 0);

    const auto mountsAfter = VfsRead::GetVFS().GetMounts();
    const auto bytesAfter = VfsRead::ReadBytes("/Engine/Shaders/RotatedFigure.hlsl");
    std::cout << "[Scenario D] После завершения второго Run() (Stop() снова вызван):\n"
              << "  Mounts count: " << mountsAfter.size() << "\n"
              << "  Bytes read: " << bytesAfter.size() << "\n";
}

// Scenario E: Mount "/Game" replacement check.
void VfsLifecycle_ScenarioE()
{
    // Не проверено во время Play:
    // BaseEngine::Play() является приватным методом. Публичный метод BaseEngine::Run()
    // выполняет StartUp -> Play -> Stop атомарно и перед возвратом всегда вызывает Stop(),
    // очищающий все маунты (Vfs.UnmountAll()).
    // В публичном API BaseEngine нет хука/коллбэка жизненного цикла (например,
    // onPlayStarted или onFrame в RuntimeLaunchOptions), позволяющего инспектировать
    // GetMounts() непосредственно во время выполнения Play() без нарушения инкапсуляции.
    std::cout << "[Scenario E] не проверено: Play() приватный, Run() атомарно вызывает Stop() перед возвратом.\n"
              << "  Необходимый публичный хук: onPlayStarted или onFrame callback в RuntimeLaunchOptions / BaseEngine.\n";

    // Проверка инварианта отсутствия дубликатов на уровне VirtualFileSystem:
    VirtualFileSystem testVfs;
    testVfs.Mount("/Game", "C:/Heuristic/GameContent");
    const auto countBefore = testVfs.GetMounts().size();
    testVfs.Mount("/Game", "C:/Explicit/GameContent");
    const auto countAfter = testVfs.GetMounts().size();

    CHECK(countBefore == 1);
    CHECK(countAfter == 1);
    CHECK(testVfs.GetMounts()[0].physicalPath == std::filesystem::path("C:/Explicit/GameContent").lexically_normal());
    std::cout << "[Scenario E] Проверено на уровне VFS: Mount(\"/Game\") обновляет путь без дубликатов (count="
              << countAfter << ").\n";
}

void VfsLifecycle_PreserveMountSet()
{
    const auto mountsBefore = VfsRead::GetVFS().GetMounts();
    CHECK(!mountsBefore.empty());

    const auto fixtureDll = FindFixtureDll();
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Game;
    options.frames = 1;
    options.module = fixtureDll;

    {
        BaseEngine engine;
        const int exitCode = engine.Run(options);
        CHECK(exitCode == 0);
    }

    const auto mountsAfter = VfsRead::GetVFS().GetMounts();
    CHECK(mountsBefore.size() == mountsAfter.size());

    for (const auto& before : mountsBefore)
    {
        bool matched = false;
        for (const auto& after : mountsAfter)
        {
            if (_stricmp(before.virtualRoot.c_str(), after.virtualRoot.c_str()) == 0 &&
                before.physicalPath == after.physicalPath)
            {
                matched = true;
                break;
            }
        }
        CHECK(matched);
    }

    std::cout << "[VFS Mount Set] Все " << mountsBefore.size()
              << " маунтов сохранены без изменений после Stop().\n";
}

void VfsLifecycle_ContentRootReverted()
{
    std::optional<std::filesystem::path> prevGamePath;
    for (const auto& m : VfsRead::GetVFS().GetMounts())
    {
        if (_stricmp(m.virtualRoot.c_str(), "/Game") == 0)
        {
            prevGamePath = m.physicalPath;
            break;
        }
    }

    const auto tempProjectContent = (EnginePaths::ExecutableDirectory() / "TestProjectContent").lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(tempProjectContent, ec);

    const auto fixtureDll = FindFixtureDll();
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Game;
    options.frames = 1;
    options.module = fixtureDll;
    options.contentRoot = tempProjectContent.string();

    {
        BaseEngine engine;
        const int exitCode = engine.Run(options);
        CHECK(exitCode == 0);
    }

    const auto mountsAfter = VfsRead::GetVFS().GetMounts();
    for (const auto& m : mountsAfter)
    {
        if (_stricmp(m.virtualRoot.c_str(), "/Game") == 0)
        {
            CHECK(m.physicalPath != tempProjectContent);
            if (prevGamePath.has_value())
            {
                CHECK(m.physicalPath == *prevGamePath);
            }
        }
    }

    if (!prevGamePath.has_value())
    {
        for (const auto& m : mountsAfter)
        {
            CHECK(_stricmp(m.virtualRoot.c_str(), "/Game") != 0);
        }
    }

    std::filesystem::remove_all(tempProjectContent, ec);
    std::cout << "[VFS ContentRoot Reverted] Проектный contentRoot не остался в /Game после Stop().\n";
}

void VfsLifecycle_PreMountedGameReverted()
{
    const auto initialGamePath = (EnginePaths::ExecutableDirectory() / "InitialPreMountedGameContent").lexically_normal();
    const auto tempProjectContent = (EnginePaths::ExecutableDirectory() / "TestProjectContent").lexically_normal();
    std::error_code ec;
    std::filesystem::create_directories(initialGamePath, ec);
    std::filesystem::create_directories(tempProjectContent, ec);

    // Pre-mount /Game explicitly via VfsRead::GetVFS()
    VfsRead::GetVFS().Mount("/Game", initialGamePath);

    const auto fixtureDll = FindFixtureDll();
    RuntimeLaunchOptions options;
    options.mode = RuntimeMode::Game;
    options.frames = 1;
    options.module = fixtureDll;
    options.contentRoot = tempProjectContent.string();

    {
        BaseEngine engine;
        const int exitCode = engine.Run(options);
        CHECK(exitCode == 0);
    }

    // Verify /Game was reverted back to initialGamePath
    bool bFoundGame = false;
    for (const auto& m : VfsRead::GetVFS().GetMounts())
    {
        if (_stricmp(m.virtualRoot.c_str(), "/Game") == 0)
        {
            bFoundGame = true;
            CHECK(m.physicalPath == initialGamePath);
        }
    }
    CHECK(bFoundGame);

    // Cleanup test /Game mount and temporary directories
    VfsRead::GetVFS().Unmount("/Game");
    std::filesystem::remove_all(initialGamePath, ec);
    std::filesystem::remove_all(tempProjectContent, ec);
    std::cout << "[VFS PreMounted Game Reverted] Предварительно смонтированный /Game восстановлен после Stop().\n";
}

