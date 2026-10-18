#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Render/ShaderCompiler.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
void CheckHR(HRESULT hr) { CHECK(SUCCEEDED(hr)); }
void Near(float actual, float expected) { CHECK(std::abs(actual - expected) < 0.0001f); }

// No window, swap chain, external assets or physical GPU needed. All tested methods are production code.
class TestGame : public Game
{
public:
    TestGame()
    {
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
        CheckHR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 1,
            D3D11_SDK_VERSION, Device.GetAddressOf(), nullptr, Context.GetAddressOf()));
        FirstPlayer = std::make_unique<Player>();
        BuildFrameConstants();
    }
    using Game::BuildFrameConstants;
    using Game::PumpMessages;
    bool PumpQuit() { bRunning = true; return PumpMessages(); }
    int GetExitCode() const { return ExitCode; }
};

class CountedLight : public PointLightComponent
{
    int& Destructions;
public:
    explicit CountedLight(int& count) : PointLightComponent(glm::vec3(0), glm::vec3(1)), Destructions(count) {}
    ~CountedLight() override { ++Destructions; }
};

void RegistryOwnership()
{
    int destructions = 0;
    TestGame game, other;
    auto light = std::make_unique<CountedLight>(destructions);
    auto* p = light.get();
    p->SetGame(&game); // Configuration before registration must remain supported.
    CHECK(game.RegisterComponent("A", p) == RegisterResult::Ok);
    light.release();
    CHECK(game.RegisterComponent("B", p) == RegisterResult::AlreadyOwned);
    CHECK(other.RegisterComponent("B", p) == RegisterResult::AlreadyOwned);
    CHECK(p->GetGame() == &game);
    CHECK(game.FindComponent("B") == nullptr && other.FindComponent("B") == nullptr);
    CHECK(game.GetPointLights().size() == 1 && other.GetPointLights().empty());
    CHECK(game.UnregisterComponent("A"));
    CHECK(destructions == 1);
    CHECK(game.GetPointLights().empty());
    CHECK(!game.UnregisterComponent("A"));
}

void RegistryFailures()
{
    TestGame game;
    Game uninitialized;
    int destructions = 0;
    auto first = std::make_unique<CountedLight>(destructions);
    auto rejected = std::make_unique<CountedLight>(destructions);
    CHECK(game.RegisterComponent("null", nullptr) == RegisterResult::NullComponent);
    CHECK(game.RegisterComponent("", rejected.get()) == RegisterResult::EmptyName);
    CHECK(uninitialized.RegisterComponent("A", rejected.get()) == RegisterResult::DeviceNotReady);
    CHECK(game.RegisterComponent("A", first.get()) == RegisterResult::Ok);
    first.release();
    CHECK(game.RegisterComponent("A", rejected.get()) == RegisterResult::DuplicateName);
    CHECK(rejected->GetGame() == nullptr && destructions == 0);
    rejected.reset();
    CHECK(destructions == 1);
}

void TransformHierarchy()
{
    GameComponent parent, child, grandchild;
    parent.SetPosition({10, 0, 0});
    parent.SetScale({2, 2, 2});
    child.SetPosition({1, 0, 0});
    CHECK(child.SetParent(&parent));
    CHECK(grandchild.SetParent(&child));
    Near(grandchild.GetWorldPosition().x, 12);
    auto version = grandchild.GetWorldVersion();
    grandchild.GetWorldMatrix();
    CHECK(version == grandchild.GetWorldVersion());
    parent.SetPosition({20, 0, 0});
    Near(grandchild.GetWorldPosition().x, 22);
    CHECK(grandchild.GetWorldVersion() > version);
    CHECK(!parent.SetParent(&grandchild));
    CHECK(parent.GetParent() == nullptr);
    CHECK(!child.SetParent(&child));
    CHECK(child.GetParent() == &parent);
    CHECK(child.SetParentWithoutScale(&parent));
    Near(child.GetWorldPosition().x, 21);
    parent.SetRotation({0, 0, 90});
    Near(child.GetWorldPosition().x, 20);
    Near(child.GetWorldPosition().y, 1);
    child.SetParent(nullptr);
    Near(child.GetWorldPosition().x, 1);
}

void UnregisterHierarchyAndInstances()
{
    TestGame game;
    auto parent = std::make_unique<GameComponent>();
    auto child = std::make_unique<GameComponent>();
    auto* p = parent.get();
    auto* c = child.get();
    CHECK(game.RegisterComponent("parent", p) == RegisterResult::Ok); parent.release();
    CHECK(game.RegisterComponent("child", c) == RegisterResult::Ok); child.release();
    CHECK(c->SetParent(p));
    auto* instance = c->CreateInstance({1, 2, 3}, {0, 0, 0}, {1, 1, 1}, {1, 1, 1, 1});
    CHECK(instance->GetGame() == &game);
    CHECK(instance->SetParent(p));
    CHECK(c->GetInstanceCount() == 1);
    instance->Update();
    CHECK(game.UnregisterComponent("parent"));
    CHECK(c->GetParent() == nullptr && instance->GetParent() == nullptr);
    Near(instance->GetWorldPosition().x, 1);
}

void LightSnapshot()
{
    TestGame game;
    auto light = std::make_unique<PointLightComponent>(glm::vec3(1, 2, 3), glm::vec3(0.2f, 0.3f, 0.4f), 2.0f, 30.0f);
    auto* p = light.get();
    CHECK(game.RegisterComponent("light", p) == RegisterResult::Ok); light.release();
    game.SetDirectionalLight({0, -2, 0}, {0.7f, 0.8f, 0.9f}, 3);
    game.BuildFrameConstants();
    const auto& frame = game.GetFrameConstants();
    Near(frame.LightMeta.x, 1);
    Near(frame.LightPositions[0].z, 3);
    Near(frame.LightParams[0].x, 2);
    Near(frame.LightDirection.y, -1);
    Near(frame.DirectionalLightColorIntensity.w, 3);
    p->SetPosition({4, 5, 6});
    Near(frame.LightPositions[0].z, 3); // Immutable until the next snapshot.
    game.BuildFrameConstants();
    Near(frame.LightPositions[0].z, 6);
    p->SetEnabled(false);
    game.BuildFrameConstants();
    Near(frame.LightMeta.x, 1); // Disabled lights retain their slot but are skipped by lighting.
    Near(frame.LightParams[0].z, 0);
}

void ShaderVariants()
{
    TestGame game;
    GameComponent component;
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(game.SupportsDeferredGeometry(&component));
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::DeferredGBuffer));
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    component.SetShaderNames(EnginePaths::Shader("ReflectiveSphere.hlsl").generic_u8string(), EnginePaths::Shader("ReflectiveSphere.hlsl").generic_u8string());
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(!game.SupportsDeferredGeometry(&component));
    CHECK(!game.BindComponentShaders(&component, ShaderCompileVariant::DeferredGBuffer));
}

ComPtr<ID3D11Buffer> Buffer(ID3D11Device* device, UINT bytes, UINT bind, const void* data = nullptr, UINT stride = 0)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = bytes; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = bind;
    if (stride) { desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; desc.StructureByteStride = stride; }
    D3D11_SUBRESOURCE_DATA initial{}; initial.pSysMem = data;
    ComPtr<ID3D11Buffer> result;
    CheckHR(device->CreateBuffer(&desc, data ? &initial : nullptr, result.GetAddressOf()));
    return result;
}

template<class T> std::vector<T> ReadBuffer(TestGame& game, ID3D11Buffer* source)
{
    D3D11_BUFFER_DESC desc{}; source->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> staging;
    CheckHR(game.GetDevice()->CreateBuffer(&desc, nullptr, staging.GetAddressOf()));
    game.GetContext()->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CheckHR(game.GetContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<T> result(desc.ByteWidth / sizeof(T));
    std::memcpy(result.data(), mapped.pData, result.size() * sizeof(T));
    game.GetContext()->Unmap(staging.Get(), 0);
    return result;
}

void ParticleShaderIndexing()
{
    TestGame game;
    auto* device = game.GetDevice(); auto* context = game.GetContext();
    ComPtr<ID3DBlob> blob; std::string errors;
    CheckHR(ShaderCompiler::CompileFromFile(EnginePaths::Shader("GPUParticleSystem.hlsl").wstring(), nullptr,
        "VSMain", "vs_5_0", blob.GetAddressOf(), &errors));
    ComPtr<ID3D11VertexShader> vs;
    CheckHR(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, vs.GetAddressOf()));
    // Stream-output captures the real vertex shader output, without rasterization or a window.
    D3D11_SO_DECLARATION_ENTRY declaration{0, "SV_POSITION", 0, 0, 4, 0};
    UINT stride = sizeof(XMFLOAT4);
    ComPtr<ID3D11GeometryShader> capture;
    CheckHR(device->CreateGeometryShaderWithStreamOutput(blob->GetBufferPointer(), blob->GetBufferSize(),
        &declaration, 1, &stride, 1, D3D11_SO_NO_RASTERIZED_STREAM, nullptr, capture.GetAddressOf()));
    GPUParticleData particles[3]{};
    for (int i = 0; i < 3; ++i) { particles[i].PositionLife = XMFLOAT4(float(i + 1), 0, 0, 1); particles[i].VelocityLifetime.w = 1; }
    GPUParticleSortPair order[3] = {{0, 2}, {0, 0}, {0, 1}};
    auto particleBuffer = Buffer(device, sizeof(particles), D3D11_BIND_SHADER_RESOURCE, particles, sizeof(GPUParticleData));
    auto orderBuffer = Buffer(device, sizeof(order), D3D11_BIND_SHADER_RESOURCE, order, sizeof(GPUParticleSortPair));
    ComPtr<ID3D11ShaderResourceView> particleSRV, orderSRV;
    CheckHR(device->CreateShaderResourceView(particleBuffer.Get(), nullptr, particleSRV.GetAddressOf()));
    CheckHR(device->CreateShaderResourceView(orderBuffer.Get(), nullptr, orderSRV.GetAddressOf()));
    GPUParticleRenderCB constants{};
    XMStoreFloat4x4(&constants.ViewMatrix, XMMatrixIdentity()); constants.ProjectionMatrix = constants.ViewMatrix;
    auto cb = Buffer(device, sizeof(constants), D3D11_BIND_CONSTANT_BUFFER);
    auto output = Buffer(device, 3 * sizeof(XMFLOAT4), D3D11_BIND_STREAM_OUTPUT);
    context->VSSetShader(vs.Get(), nullptr, 0); context->GSSetShader(capture.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    for (bool sorted : {false, true, false})
    {
        constants.SortingEnabled = sorted ? 1u : 0u;
        context->UpdateSubresource(cb.Get(), 0, nullptr, &constants, 0, 0);
        // No sort SRV at all in unsorted mode: no dependency on uninitialized/stale contents.
        ID3D11ShaderResourceView* views[] = {particleSRV.Get(), sorted ? orderSRV.Get() : nullptr};
        context->VSSetShaderResources(0, 2, views);
        UINT offset = 0; context->SOSetTargets(1, output.GetAddressOf(), &offset);
        context->DrawInstanced(1, 3, 0, 0);
        context->SOSetTargets(0, nullptr, nullptr);
        auto positions = ReadBuffer<XMFLOAT4>(game, output.Get());
        for (int i = 0; i < 3; ++i) Near(positions[i].x, float((sorted ? order[i].ParticleIndex : i) + 1));
    }
}

void ParticleComponentModes()
{
    TestGame game;
    ParticleSystemComponent particles;
    particles.SetGame(&game); particles.SetParticleCount(3);
    particles.SetDepthCollision(false); particles.CreateBuffers(game.GetDevice());
    for (bool sorted : {false, true, false})
    {
        particles.SetSortingEnabled(sorted); particles.Tick(0.016f);
        game.GetFrameStats().Reset();
        particles.DispatchCompute(game.GetContext());
        CHECK(game.GetFrameStats().Dispatches == (sorted ? 23u : 1u)); // count clamps to 64: keys + 21 sort stages
        particles.Render(game.GetContext());
        CHECK(game.GetFrameStats().DrawCalls == 1);
        CHECK(game.GetFrameStats().InstancesDrawn == 64);
        ComPtr<ID3D11Buffer> constants;
        game.GetContext()->VSGetConstantBuffers(0, 1, constants.GetAddressOf());
        CHECK(constants != nullptr);
        auto data = ReadBuffer<GPUParticleRenderCB>(game, constants.Get());
        CHECK(data[0].SortingEnabled == (sorted ? 1u : 0u));
        game.SetSimulationEnabled(false);
        game.GetFrameStats().Reset();
        particles.DispatchCompute(game.GetContext());
        CHECK(game.GetFrameStats().Dispatches == (sorted ? 22u : 0u));
        game.SetSimulationEnabled(true);
    }
}

void QuitMessage()
{
    TestGame game;
    PostQuitMessage(17);
    CHECK(!game.PumpQuit());
    CHECK(game.GetExitCode() == 17);
}

#ifdef DeleteFile
#undef DeleteFile
#endif
void VirtualFileSystemTests()
{
    VirtualFileSystem vfs;
    const auto engineContent = EnginePaths::ContentRoot();

    // Normalization check
    CHECK(VirtualFileSystem::NormalizeVirtualPath("//Engine\\\\Shaders/../Shaders//foo.hlsl") == "/Engine/Shaders/foo.hlsl");
    CHECK(VirtualFileSystem::NormalizeVirtualPath("/Engine/../../secret") == "/secret");
    CHECK(VirtualFileSystem::NormalizeVirtualPath("") == "/");

    // Mount & Resolve check
    vfs.Mount("/Engine", engineContent);
    const auto resolved = vfs.Resolve("/Engine/Shaders/RotatedFigure.hlsl");
    CHECK(resolved.has_value());
    CHECK(vfs.Exists("/Engine/Shaders/RotatedFigure.hlsl"));

    // Case-insensitivity check on Windows
    const auto resolvedLower = vfs.Resolve("/engine/shaders/rotatedfigure.hlsl");
    CHECK(resolvedLower.has_value());

    // ReadText check
    const std::string shaderText = vfs.ReadText("/Engine/Shaders/RotatedFigure.hlsl");
    CHECK(!shaderText.empty());
    CHECK(shaderText.find("RotatedSpecularWeight") != std::string::npos);

    // ReadBytes check
    const auto shaderBytes = vfs.ReadBytes("/Engine/Shaders/RotatedFigure.hlsl");
    CHECK(!shaderBytes.empty());
    CHECK(shaderBytes.size() >= shaderText.size());

    // Asset classification tests
    CHECK(DeduceAssetType(".fbx") == AssetType::Model);
    CHECK(DeduceAssetType("mesh.obj") == AssetType::Model);
    CHECK(DeduceAssetType("scene.gltf") == AssetType::Model);
    CHECK(DeduceAssetType("scene.glb") == AssetType::Model);
    CHECK(DeduceAssetType("char.blend") == AssetType::Model);
    CHECK(DeduceAssetType("asset.dae") == AssetType::Model);

    CHECK(DeduceAssetType(".png") == AssetType::Texture);
    CHECK(DeduceAssetType("diffuse.JPG") == AssetType::Texture);
    CHECK(DeduceAssetType("photo.jpeg") == AssetType::Texture);
    CHECK(DeduceAssetType("norm.tga") == AssetType::Texture);
    CHECK(DeduceAssetType("sky.dds") == AssetType::Texture);
    CHECK(DeduceAssetType("icon.bmp") == AssetType::Texture);
    CHECK(DeduceAssetType("env.hdr") == AssetType::Texture);

    CHECK(DeduceAssetType("shader.hlsl") == AssetType::Shader);
    CHECK(DeduceAssetType("effect.fx") == AssetType::Shader);
    CHECK(DeduceAssetType("code.glsl") == AssetType::Shader);
    CHECK(DeduceAssetType("compiled.cso") == AssetType::Shader);

    CHECK(DeduceAssetType("script.lua") == AssetType::Script);
    CHECK(DeduceAssetType("config.json") == AssetType::Config);
    CHECK(DeduceAssetType("settings.ini") == AssetType::Config);
    CHECK(DeduceAssetType("layout.xml") == AssetType::Config);
    CHECK(DeduceAssetType("notes.txt") == AssetType::Config);

    CHECK(DeduceAssetType("sound.wav") == AssetType::Audio);
    CHECK(DeduceAssetType("music.ogg") == AssetType::Audio);
    CHECK(DeduceAssetType("song.mp3") == AssetType::Audio);
    CHECK(DeduceAssetType("track.flac") == AssetType::Audio);

    CHECK(DeduceAssetType("unknown.dat") == AssetType::Unknown);
    CHECK(DeduceAssetType("") == AssetType::Unknown);

    CHECK(vfs.GetAssetType("/Engine/Shaders/RotatedFigure.hlsl") == AssetType::Shader);
    CHECK(vfs.GetAssetType("/Game/Textures/Wood.png") == AssetType::Texture);

    // GetFileInfo test
    const auto infoOpt = vfs.GetFileInfo("/Engine/Shaders/RotatedFigure.hlsl");
    CHECK(infoOpt.has_value());
    const auto& info = *infoOpt;
    CHECK(info.virtualPath == "/Engine/Shaders/RotatedFigure.hlsl");
    CHECK(info.type == AssetType::Shader);
    CHECK(info.extension == ".hlsl");
    CHECK(info.sizeBytes > 0);
    CHECK(std::filesystem::exists(info.physicalPath));
    CHECK(!vfs.GetFileInfo("/Engine/Shaders/NoSuchShader.hlsl").has_value());

    // EnumerateFiles test
    const auto allEngineFiles = vfs.EnumerateFiles("/Engine", true);
    CHECK(!allEngineFiles.empty());
    bool foundRotated = false;
    for (const auto& file : allEngineFiles)
    {
        if (file.virtualPath == "/Engine/Shaders/RotatedFigure.hlsl")
        {
            foundRotated = true;
            CHECK(file.type == AssetType::Shader);
            CHECK(file.sizeBytes > 0);
        }
    }
    CHECK(foundRotated);

    const auto shaderDirFiles = vfs.EnumerateFiles("/Engine/Shaders", false);
    CHECK(!shaderDirFiles.empty());

    // FindFiles tests (type filter, pattern filter, and combined)
    const auto shaderFiles = vfs.FindFiles("/Engine", AssetType::Shader, "", true);
    CHECK(!shaderFiles.empty());
    for (const auto& f : shaderFiles)
    {
        CHECK(f.type == AssetType::Shader);
    }

    const auto figureFiles = vfs.FindFiles("/Engine", AssetType::Unknown, "RotatedFigure*", true);
    CHECK(!figureFiles.empty());
    CHECK(figureFiles[0].virtualPath == "/Engine/Shaders/RotatedFigure.hlsl");

    const auto hlslShaders = vfs.FindFiles("/Engine", AssetType::Shader, "*.hlsl", true);
    CHECK(!hlslShaders.empty());
    for (const auto& f : hlslShaders)
    {
        CHECK(f.type == AssetType::Shader);
        CHECK(f.extension == ".hlsl");
    }

    const auto audioFiles = vfs.FindFiles("/Engine", AssetType::Audio, "", true);
    CHECK(audioFiles.empty());

    // ImportFile tests
    const auto tempRoot = std::filesystem::temp_directory_path() / "EngineVfsTest";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot / "External", ec);
    std::filesystem::create_directories(tempRoot / "GameContent", ec);

    const auto externalFile = tempRoot / "External" / "ImportedModel.obj";
    {
        std::ofstream out(externalFile);
        out << "v 0.0 0.0 0.0\nv 1.0 1.0 1.0\n";
    }

    vfs.Mount("/Game", tempRoot / "GameContent");

    const auto imported = vfs.ImportFile(externalFile, "/Game/Meshes");
    CHECK(imported.has_value());
    CHECK(imported->virtualPath == "/Game/Meshes/ImportedModel.obj");
    CHECK(imported->type == AssetType::Model);
    CHECK(imported->extension == ".obj");
    CHECK(imported->sizeBytes > 0);
    CHECK(std::filesystem::exists(imported->physicalPath));
    CHECK(vfs.Exists("/Game/Meshes/ImportedModel.obj"));

    const std::string importedText = vfs.ReadText("/Game/Meshes/ImportedModel.obj");
    CHECK(importedText.find("v 1.0 1.0 1.0") != std::string::npos);

    CHECK(!vfs.ImportFile(tempRoot / "External" / "NonExistent.fbx", "/Game/Meshes").has_value());
    CHECK(!vfs.ImportFile(externalFile, "/UnmountedRoot/Meshes").has_value());

    // Directory and file manipulation tests (CreateDir, WriteText, WriteBytes, Copy, Move, DeleteFile, DeleteDir)
    // Root protection checks
    CHECK(!vfs.DeleteFile("/"));
    CHECK(!vfs.DeleteDir("/"));
    CHECK(!vfs.DeleteFile("/Engine"));
    CHECK(!vfs.DeleteDir("/Engine"));
    CHECK(!vfs.DeleteFile("/Game"));
    CHECK(!vfs.DeleteDir("/Game"));
    CHECK(!vfs.WriteText("/", "fail"));
    CHECK(!vfs.WriteText("/Engine", "fail"));

    // CreateDir test
    CHECK(vfs.CreateDir("/Game/WorkDir/Nested"));
    CHECK(vfs.Exists("/Game/WorkDir/Nested"));
    CHECK(vfs.CreateDir("/Game/WorkDir/Nested")); // idempotent

    // WriteText & WriteBytes tests
    CHECK(vfs.WriteText("/Game/WorkDir/notes.txt", "AlphaBetaGamma"));
    CHECK(vfs.Exists("/Game/WorkDir/notes.txt"));
    CHECK(vfs.ReadText("/Game/WorkDir/notes.txt") == "AlphaBetaGamma");

    const std::vector<uint8_t> testBytes = { 0xDE, 0xAD, 0xBE, 0xEF };
    CHECK(vfs.WriteBytes("/Game/WorkDir/data.bin", testBytes));
    CHECK(vfs.ReadBytes("/Game/WorkDir/data.bin") == testBytes);

    // Copy test
    CHECK(vfs.Copy("/Game/WorkDir/notes.txt", "/Game/WorkDir/notes_copy.txt"));
    CHECK(vfs.Exists("/Game/WorkDir/notes_copy.txt"));
    CHECK(vfs.ReadText("/Game/WorkDir/notes_copy.txt") == "AlphaBetaGamma");

    // Move test
    CHECK(vfs.Move("/Game/WorkDir/notes_copy.txt", "/Game/WorkDir/notes_moved.txt"));
    CHECK(!vfs.Exists("/Game/WorkDir/notes_copy.txt"));
    CHECK(vfs.Exists("/Game/WorkDir/notes_moved.txt"));
    CHECK(vfs.ReadText("/Game/WorkDir/notes_moved.txt") == "AlphaBetaGamma");

    // DeleteFile test
    CHECK(vfs.DeleteFile("/Game/WorkDir/notes_moved.txt"));
    CHECK(!vfs.Exists("/Game/WorkDir/notes_moved.txt"));
    CHECK(!vfs.DeleteFile("/Game/WorkDir/notes_moved.txt")); // already deleted

    // DeleteDir test
    CHECK(vfs.DeleteDir("/Game/WorkDir/Nested", false)); // delete empty dir
    CHECK(!vfs.Exists("/Game/WorkDir/Nested"));
    CHECK(vfs.DeleteDir("/Game/WorkDir", true)); // recursive delete
    CHECK(!vfs.Exists("/Game/WorkDir"));

    std::filesystem::remove_all(tempRoot, ec);

    // Unmount check
    CHECK(vfs.Unmount("/Engine"));
    CHECK(!vfs.Exists("/Engine/Shaders/RotatedFigure.hlsl"));
    CHECK(!vfs.Resolve("/Engine/Shaders/RotatedFigure.hlsl").has_value());
}

void SceneOwnershipTests();
void SceneHierarchyTests();
void SceneDeferredTests();

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"scene ownership and IDs (no GPU)", SceneOwnershipTests},
        {"scene hierarchy and snapshots (no GPU)", SceneHierarchyTests},
        {"scene deferred mutations (no GPU)", SceneDeferredTests},
        {"registry ownership", RegistryOwnership}, {"registration failure ownership", RegistryFailures},
        {"transform hierarchy and cache", TransformHierarchy}, {"unregister hierarchy and instances", UnregisterHierarchyAndInstances},
        {"frame light snapshot", LightSnapshot}, {"forward/deferred shader variants", ShaderVariants},
        {"particle GPU indexing", ParticleShaderIndexing}, {"particle component sorting modes", ParticleComponentModes},
        {"WM_QUIT exit code", QuitMessage}, {"virtual file system (VFS)", VirtualFileSystemTests}
    };
    int failures = 0;
    for (const auto& test : tests)
    {
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL: " << test.first << ": " << error.what() << '\n'; }
    }
    std::cout << std::size(tests) - failures << '/' << std::size(tests) << " tests passed\n";
    return failures ? 1 : 0;
}
