#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Render/MeshValidation.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Render/ShaderCompiler.h>
#include "../Source/Resources/VfsRead.h"
#include "VfsLifecycleTests.h"
#include <algorithm>
#include "VfsServiceTests.h"
#include "VfsProxyLifecycleTests.h"
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
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
    using Game::CreateInputLayouts;
    using Game::PumpMessages;
    void SetCamera(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
    {
        const XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&at), XMLoadFloat3(&up));
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1.0f, 0.1f, 100.0f);
        XMStoreFloat4x4(&CurrentFrame.viewMatrix, XMMatrixTranspose(view));
        XMStoreFloat4x4(&CurrentFrame.projectionMatrix, XMMatrixTranspose(projection));
        CurrentFrame.CameraPosition = XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);
        // Every point is inside: tests choose what is on screen by placement, not by culling.
        for (XMFLOAT4& plane : CurrentFrame.FrustumPlanes)
        {
            plane = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
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

struct TempShaderDir
{
    std::filesystem::path Root = std::filesystem::temp_directory_path() / "EngineTestsShaders";
    TempShaderDir() { std::filesystem::remove_all(Root); std::filesystem::create_directories(Root / "Inc"); }
    ~TempShaderDir() { std::error_code ec; std::filesystem::remove_all(Root, ec); }
    std::string Write(const std::string& name, const std::string& text) const
    {
        const auto path = Root / name;
        std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
        return path.generic_u8string();
    }
};

void ShaderCompilerIncludes()
{
    TempShaderDir dir;
    const std::string helper = dir.Write("Inc/Helper.hlsli", "float4 Helper() { return float4(1, 0, 0, 1); }\n");
    const std::string surfaceA = dir.Write("SurfaceA.hlsl", "float4 Surface() { return Helper() * TINT; }\n");
    const std::string main = dir.Write("Main.hlsl",
        "#include \"Inc/Helper.hlsli\"\n#include \"MaterialSurface.hlsli\"\nfloat4 PSMain() : SV_Target { return Surface(); }\n");

    ShaderCompiler::CompileOptions options;
    options.Defines = {{"TINT", "0.5f"}};
    options.IncludeOverrides["MaterialSurface.hlsli"] = surfaceA;
    ShaderCompiler::CompileResult result = ShaderCompiler::Compile(main, "PSMain", "ps_5_0", options);
    CHECK(result.Succeeded());
    CHECK(result.Dependencies.size() == 3);
    CHECK(result.Dependencies[0] == ShaderCompiler::NormalizePath(main));
    CHECK(result.Dependencies[1] == ShaderCompiler::NormalizePath(helper));
    CHECK(result.Dependencies[2] == ShaderCompiler::NormalizePath(surfaceA));

    // Without the define the surface does not compile; the message names the overridden file.
    options.Defines.clear();
    result = ShaderCompiler::Compile(main, "PSMain", "ps_5_0", options);
    CHECK(!result.Succeeded() && !result.Blob);
    CHECK(result.Errors.find("SurfaceA.hlsl") != std::string::npos);

    options.IncludeOverrides["MaterialSurface.hlsli"] = (dir.Root / "Missing.hlsl").generic_u8string();
    CHECK(!ShaderCompiler::Compile(main, "PSMain", "ps_5_0", options).Succeeded());
    CHECK(!ShaderCompiler::Compile((dir.Root / "Absent.hlsl").generic_u8string(), "PSMain", "ps_5_0").Succeeded());
    CHECK(ShaderCompiler::NormalizePath(dir.Root.u8string() + "\\Inc\\..\\MAIN.hlsl") == ShaderCompiler::NormalizePath(main));
}

void ShaderInvalidation()
{
    TempShaderDir dir;
    const std::string shared = dir.Write("Inc/Shared.hlsli", "float4 Shade() { return float4(0, 1, 0, 1); }\n");
    const char* goodShader =
        "#include \"Inc/Shared.hlsli\"\n"
        "float4 VSMain(float4 pos : POSITION, float4 col : COLOR) : SV_Position { return pos; }\n"
        "float4 PSMain() : SV_Target { return Shade(); }\n";
    const std::string shader = dir.Write("Hot.hlsl", goodShader);

    TestGame game;
    // Registered: invalidation re-resolves only components the scene owns.
    CHECK(game.RegisterComponent("hot", new GameComponent(), shader, shader) == RegisterResult::Ok);
    GameComponent& component = *game.FindComponent("hot");
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(game.GetShaderErrors(shader).empty());
    CHECK(!game.InvalidateShaderFile((dir.Root / "Unrelated.hlsl").generic_u8string()));

    // Edits on disk are invisible until the file is invalidated.
    dir.Write("Hot.hlsl", "float4 PSMain() : SV_Target { return undefinedValue; }\n");
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(game.InvalidateShaderFile(shader));
    game.BindComponentShaders(&component, ShaderCompileVariant::Default);
    const std::string errors = game.GetShaderErrors(shader);
    CHECK(errors.find("undefinedValue") != std::string::npos);
    CHECK(errors.find("Hot.hlsl") != std::string::npos);

    // A failed compilation is retried after invalidation, and an include invalidates its users.
    dir.Write("Hot.hlsl", goodShader);
    CHECK(game.InvalidateShaderFile(shader));
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(game.GetShaderErrors(shader).empty());
    CHECK(game.InvalidateShaderFile(shared));
    CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    CHECK(game.GetShaderErrors(shader).empty());

    // Full reload (F6 in the editor): an edit becomes visible without naming the file.
    dir.Write("Hot.hlsl", "float4 PSMain() : SV_Target { return missingAfterReload; }\n");
    game.InvalidateAllShaders();
    game.BindComponentShaders(&component, ShaderCompileVariant::Default);
    CHECK(game.GetShaderErrors(shader).find("missingAfterReload") != std::string::npos);
}

ComPtr<ID3D11ShaderReflection> Reflect(ID3DBlob* blob)
{
    ComPtr<ID3D11ShaderReflection> reflection;
    CheckHR(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_ID3D11ShaderReflection,
                       reinterpret_cast<void**>(reflection.GetAddressOf())));
    return reflection;
}

UINT RenderTargetCount(ID3DBlob* pixelShader)
{
    ComPtr<ID3D11ShaderReflection> reflection = Reflect(pixelShader);
    D3D11_SHADER_DESC desc{};
    reflection->GetDesc(&desc);
    UINT targets = 0;
    for (UINT i = 0; i < desc.OutputParameters; ++i)
    {
        D3D11_SIGNATURE_PARAMETER_DESC parameter{};
        reflection->GetOutputParameterDesc(i, &parameter);
        targets += parameter.SystemValueType == D3D_NAME_TARGET ? 1 : 0;
    }
    return targets;
}

ShaderCompiler::CompileResult CompileMaterial(const std::string& surface, const char* entry, const char* target,
                                              std::vector<std::pair<std::string, std::string>> defines = {})
{
    ShaderCompiler::CompileOptions options;
    options.Defines = std::move(defines);
    options.IncludeOverrides["MaterialSurface.hlsli"] = surface;
    return ShaderCompiler::Compile(EnginePaths::Shader("Material/MaterialTemplate.hlsl").generic_u8string(),
                                   entry, target, options);
}

void MaterialTemplateVariants()
{
    const std::string defaultSurface = EnginePaths::Shader("Material/DefaultSurface.hlsl").generic_u8string();
    const std::string errorSurface = EnginePaths::Shader("Material/ErrorSurface.hlsl").generic_u8string();
    const std::pair<std::string, std::string> gbuffer{"DEFERRED_GBUFFER", "1"};

    CHECK(CompileMaterial(defaultSurface, "VSMain", "vs_5_0").Succeeded());
    auto expectTargets = [&](const std::string& surface, std::vector<std::pair<std::string, std::string>> defines, UINT forward, UINT deferred)
    {
        auto plain = CompileMaterial(surface, "PSMain", "ps_5_0", defines);
        defines.push_back(gbuffer);
        auto geometry = CompileMaterial(surface, "PSMain", "ps_5_0", defines);
        if (!plain.Succeeded() || !geometry.Succeeded()) std::cerr << plain.Errors << geometry.Errors;
        CHECK(plain.Succeeded() && geometry.Succeeded());
        CHECK(RenderTargetCount(plain.Blob.Get()) == forward);
        CHECK(RenderTargetCount(geometry.Blob.Get()) == deferred);
    };

    // Lit opaque/masked materials write the G-buffer; everything else stays forward-only.
    expectTargets(defaultSurface, {}, 1, 3);
    expectTargets(defaultSurface, {{"MATERIAL_BLEND", "1"}, {"MATERIAL_ALPHA_CLIP", "0.3f"}, {"MATERIAL_TWO_SIDED", "1"}}, 1, 3);
    expectTargets(defaultSurface, {{"MATERIAL_BLEND", "2"}}, 1, 1);
    expectTargets(defaultSurface, {{"MATERIAL_BLEND", "3"}}, 1, 1);
    expectTargets(defaultSurface, {{"MATERIAL_UNLIT", "1"}}, 1, 1);
    expectTargets(errorSurface, {}, 1, 1); // declares MATERIAL_UNLIT itself

    // Pass variants: instanced and skinned (same targets), depth-only shadow, two-target atlas bake.
    expectTargets(defaultSurface, {{"MATERIAL_INSTANCED", "1"}}, 1, 3);
    expectTargets(defaultSurface, {{"MATERIAL_SKINNED", "1"}}, 1, 3);
    expectTargets(defaultSurface, {{"MATERIAL_SKINNED", "1"}, {"MATERIAL_INSTANCED", "1"}}, 1, 3);
    for (const auto& defines : std::vector<std::vector<std::pair<std::string, std::string>>>{
             {{"MATERIAL_INSTANCED", "1"}}, {{"MATERIAL_SKINNED", "1"}}, {{"MATERIAL_SKINNED", "1"}, {"MATERIAL_INSTANCED", "1"}}})
    {
        CHECK(CompileMaterial(defaultSurface, "VSMain", "vs_5_0", defines).Succeeded());
    }
    auto shadow = CompileMaterial(defaultSurface, "PSMain", "ps_5_0",
                                  {{"MATERIAL_BLEND", "1"}, {"MATERIAL_INSTANCED", "1"}, {"MATERIAL_SHADOW", "1"}});
    CHECK(shadow.Succeeded() && RenderTargetCount(shadow.Blob.Get()) == 0);
    auto bake = CompileMaterial(defaultSurface, "PSMain", "ps_5_0", {{"MATERIAL_IMPOSTOR_BAKE", "1"}});
    CHECK(bake.Succeeded() && RenderTargetCount(bake.Blob.Get()) == 2);

    TempShaderDir dir;
    const std::string emissive = dir.Write("Emissive.hlsl",
        "#define MATERIAL_EMISSIVE\n"
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Emissive = float3(1, 0.5f, 0) * (sin(i.Time) * 0.5f + 0.5f); }\n");
    expectTargets(emissive, {}, 1, 1);

    // Parameters and textures of a surface are visible to reflection under their own names.
    const std::string parameters = dir.Write("Params.hlsl",
        "cbuffer MaterialParams : register(b1) { float4 Tint; float Gloss; float2 UVScale; };\n"
        "Texture2D BaseMap : register(t6);\n"
        "void Surface(MaterialInput i, inout MaterialOutput o)\n"
        "{\n"
        "    o.Albedo = BaseMap.Sample(MaterialSampler, i.UV * UVScale).rgb * Tint.rgb;\n"
        "    o.SpecularWeight = Gloss;\n"
        "}\n");
    auto compiled = CompileMaterial(parameters, "PSMain", "ps_5_0");
    CHECK(compiled.Succeeded());
    ComPtr<ID3D11ShaderReflection> reflection = Reflect(compiled.Blob.Get());
    D3D11_SHADER_INPUT_BIND_DESC binding{};
    CheckHR(reflection->GetResourceBindingDescByName("MaterialParams", &binding));
    CHECK(binding.Type == D3D_SIT_CBUFFER && binding.BindPoint == 1);
    CheckHR(reflection->GetResourceBindingDescByName("BaseMap", &binding));
    CHECK(binding.Type == D3D_SIT_TEXTURE && binding.BindPoint == 6);
    ID3D11ShaderReflectionConstantBuffer* params = reflection->GetConstantBufferByName("MaterialParams");
    D3D11_SHADER_VARIABLE_DESC variable{};
    CheckHR(params->GetVariableByName("Gloss")->GetDesc(&variable));
    CHECK(variable.StartOffset == 16 && variable.Size == 4);
    CheckHR(params->GetVariableByName("UVScale")->GetDesc(&variable));
    CHECK(variable.StartOffset == 20 && variable.Size == 8);

    // Errors of the author point at the surface file, not at the template.
    const std::string broken = dir.Write("Broken.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o)\n{\n    o.Albedo = Tnt.rgb;\n}\n");
    compiled = CompileMaterial(broken, "PSMain", "ps_5_0");
    CHECK(!compiled.Succeeded());
    CHECK(compiled.Errors.find("Broken.hlsl(3,") != std::string::npos);
    CHECK(compiled.Errors.find("Tnt") != std::string::npos);
}

void CheckMesh(const MeshLodData& mesh)
{
    CHECK(!mesh.Vertices.empty() && !mesh.Indices.empty() && mesh.Indices.size() % 3 == 0);
    for (const MeshVertex& vertex : mesh.Vertices)
    {
        Near(XMVectorGetX(XMVector3Length(XMLoadFloat4(&vertex.Normal))), 1.0f);
        CHECK(vertex.Position.w == 1.0f && vertex.Normal.w == 0.0f);
        CHECK(vertex.TexCoord.x >= 0.0f && vertex.TexCoord.x <= 1.0f && vertex.TexCoord.y >= 0.0f && vertex.TexCoord.y <= 1.0f);
    }
    for (size_t i = 0; i < mesh.Indices.size(); i += 3)
    {
        const uint32_t a = mesh.Indices[i], b = mesh.Indices[i + 1], c = mesh.Indices[i + 2];
        CHECK(a < mesh.Vertices.size() && b < mesh.Vertices.size() && c < mesh.Vertices.size());
        const XMVECTOR pa = XMLoadFloat4(&mesh.Vertices[a].Position);
        const XMVECTOR cross = XMVector3Cross(XMVectorSubtract(XMLoadFloat4(&mesh.Vertices[b].Position), pa),
                                              XMVectorSubtract(XMLoadFloat4(&mesh.Vertices[c].Position), pa));
        const XMVECTOR normal = XMVectorAdd(XMVectorAdd(XMLoadFloat4(&mesh.Vertices[a].Normal), XMLoadFloat4(&mesh.Vertices[b].Normal)),
                                            XMLoadFloat4(&mesh.Vertices[c].Normal));
        // Clockwise from the front in the left-handed engine: the cross product points along the normal.
        CHECK(XMVectorGetX(XMVector3Dot(cross, normal)) > 0.0f);
    }
}

void MeshPrimitivesGeometry()
{
    const MeshLodData sphere = MeshPrimitives::Sphere(2.0f, 16, 8);
    CheckMesh(sphere);
    CHECK(sphere.Vertices.size() == 17u * 9u);
    CHECK(sphere.Indices.size() == (16u * 8u * 2u - 2u * 16u) * 3u); // pole quads are single triangles
    for (const MeshVertex& vertex : sphere.Vertices)
    {
        Near(XMVectorGetX(XMVector3Length(XMLoadFloat4(&vertex.Position))), 2.0f);
    }

    const MeshLodData cube = MeshPrimitives::Cube(2.0f);
    CheckMesh(cube);
    CHECK(cube.Vertices.size() == 24 && cube.Indices.size() == 36);
    const MeshBounds cubeBounds = ComputeMeshBounds(cube);
    Near(cubeBounds.Min.x, -1.0f); Near(cubeBounds.Max.y, 1.0f); Near(cubeBounds.Max.z, 1.0f);

    const MeshLodData plane = MeshPrimitives::Plane(4.0f, 2.0f, 3);
    CheckMesh(plane);
    CHECK(plane.Vertices.size() == 16 && plane.Indices.size() == 3u * 3u * 6u);
    const MeshBounds planeBounds = ComputeMeshBounds(plane);
    Near(planeBounds.Max.x, 2.0f); Near(planeBounds.Max.z, 1.0f); Near(planeBounds.Max.y, 0.0f);

    // Unit shapes of the scene catalog: 1 x 1 x 1 around the origin.
    for (const MeshLodData& shape : {MeshPrimitives::Cylinder(), MeshPrimitives::Cone(), MeshPrimitives::Pyramid()})
    {
        CheckMesh(shape);
        const MeshBounds bounds = ComputeMeshBounds(shape);
        Near(bounds.Min.y, -0.5f); Near(bounds.Max.y, 0.5f); Near(bounds.Max.x, 0.5f); Near(bounds.Min.z, -0.5f);
    }
}

// Draws one registered component into a 64x64 target with the given culling; returns RGBA8 pixels.
// prepare (optional) binds shaders instead of the component's own pair.
std::vector<uint32_t> RenderOffscreen(TestGame& game, GameComponent& component, D3D11_CULL_MODE cull,
                                      const std::function<void(ID3D11DeviceContext*)>& prepare = {})
{
    constexpr UINT Size = 64;
    ID3D11Device* device = game.GetDevice();
    ID3D11DeviceContext* context = game.GetContext();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = Size; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11RenderTargetView> rtv;
    CheckHR(device->CreateTexture2D(&desc, nullptr, target.GetAddressOf()));
    CheckHR(device->CreateRenderTargetView(target.Get(), nullptr, rtv.GetAddressOf()));

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID; rasterDesc.CullMode = cull; rasterDesc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> raster;
    CheckHR(device->CreateRasterizerState(&rasterDesc, raster.GetAddressOf()));

    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->ClearState();
    context->ClearRenderTargetView(rtv.Get(), clear);
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(Size), static_cast<float>(Size), 0.0f, 1.0f};
    context->RSSetViewports(1, &viewport);
    context->RSSetState(raster.Get());
    if (prepare)
    {
        prepare(context);
    }
    else
    {
        CHECK(game.BindComponentShaders(&component, ShaderCompileVariant::Default));
    }
    component.Update();
    component.Render(context);

    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    CheckHR(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()));
    context->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CheckHR(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint32_t> pixels(Size * Size);
    for (UINT y = 0; y < Size; ++y)
    {
        std::memcpy(&pixels[y * Size], static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch, Size * sizeof(uint32_t));
    }
    context->Unmap(staging.Get(), 0);
    return pixels;
}

void MeshComponentRendering()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    const std::string shader = EnginePaths::Shader("BaseFBX.hlsl").generic_u8string(); // a Mesh-format material
    game.SetCamera({0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});

    auto* plane = new MeshComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    plane->SetMesh(MeshPrimitives::Plane(2.0f, 2.0f));
    CHECK(game.RegisterComponent("plane", plane, shader, shader) == RegisterResult::Ok);
    Near(plane->GetLocalBoundingRadius(), std::sqrt(2.0f));

    // Seen from above the plane covers the centre but not the corners, and only its front faces exist.
    std::vector<uint32_t> pixels = RenderOffscreen(game, *plane, D3D11_CULL_BACK);
    CHECK(pixels[32 * 64 + 32] != 0 && pixels[0] == 0);
    CHECK(game.GetFrameStats().DrawCalls == 1);
    pixels = RenderOffscreen(game, *plane, D3D11_CULL_FRONT);
    CHECK(pixels[32 * 64 + 32] == 0);

    // Geometry can be replaced after registration.
    plane->SetMesh(MeshPrimitives::Sphere(0.5f));
    pixels = RenderOffscreen(game, *plane, D3D11_CULL_BACK);
    CHECK(pixels[32 * 64 + 32] != 0 && pixels[32 * 64 + 2] == 0);
    Near(plane->GetLocalBoundingRadius(), 0.5f);
}

const char* const ParamsSurface =
    "cbuffer MaterialParams : register(b1)\n"
    "{\n"
    "    float4 Tint = float4(1, 0, 0, 1);\n"
    "    float Gloss = 0.5f;\n"
    "    float2 UVScale = float2(2, 2);\n"
    "    float4x4 Transform;\n"
    "};\n"
    "Texture2D BaseMap : register(t6);\n"
    "void Surface(MaterialInput i, inout MaterialOutput o)\n"
    "{\n"
    "    float2 uv = mul(float4(i.UV * UVScale, 0, 1), Transform).xy;\n"
    "    o.Albedo = BaseMap.Sample(MaterialSampler, uv).rgb * Tint.rgb;\n"
    "    o.SpecularWeight = Gloss;\n"
    "}\n";

void MaterialCompilationAndParameters()
{
    TestGame game;
    TempShaderDir dir;
    const std::string surface = dir.Write("Params.hlsl", ParamsSurface);
    Material material(surface);
    CHECK(material.NeedsCompile() && !material.IsCompiled());
    CHECK(material.Compile(game.GetDevice(), game.GetContext()));
    CHECK(!material.HasErrors() && material.IsCompiled() && material.WritesGBuffer() && !material.NeedsCompile());
    CHECK(material.DependsOn(surface));
    CHECK(material.DependsOn(EnginePaths::Shader("Common.hlsli").generic_u8string()));

    // Reflection: names, types, offsets and HLSL initializers.
    CHECK(material.GetParameters().size() == 4);
    const MaterialParameter* tint = material.FindParameter("Tint");
    CHECK(tint && tint->Type == MaterialParameterType::Float4 && tint->Offset == 0);
    CHECK(tint->Default == (std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f}));
    const MaterialParameter* gloss = material.FindParameter("Gloss");
    CHECK(gloss && gloss->Type == MaterialParameterType::Float && gloss->Offset == 16);
    Near(gloss->Default[0], 0.5f);
    CHECK(material.FindParameter("UVScale")->Type == MaterialParameterType::Float2);
    CHECK(material.FindParameter("Transform")->Type == MaterialParameterType::Unsupported);
    CHECK(!material.SetParameter("Transform", {}) && !material.SetParameter("Missing", {}));
    CHECK(material.GetTextures().size() == 1 && material.GetTextures()[0].Name == "BaseMap" && material.GetTextures()[0].Slot == 6);

    std::array<float, 4> value{};
    CHECK(material.GetParameter("UVScale", value) && value[0] == 2.0f && value[1] == 2.0f);
    CHECK(material.SetParameter("Tint", {0.0f, 1.0f, 0.0f, 1.0f}));
    CHECK(material.SetParameter("Gloss", {0.9f, 7.0f, 7.0f, 7.0f})); // extra components are ignored
    CHECK(material.GetParameter("Gloss", value) && value[0] == 0.9f && value[1] == 0.0f);

    // An edit keeps values whose name and type survived; a changed type falls back to its default.
    std::string edited = ParamsSurface;
    edited.replace(edited.find("float Gloss = 0.5f;"), 19, "float2 Gloss = float2(0.25f, 0.75f); float Extra = 3;");
    edited.replace(edited.find("o.SpecularWeight = Gloss;"), 25, "o.SpecularWeight = Gloss.x * Extra;");
    dir.Write("Params.hlsl", edited);
    CHECK(material.Compile(game.GetDevice(), game.GetContext()));
    CHECK(material.GetParameters().size() == 5);
    CHECK(material.GetParameter("Tint", value) && value == (std::array<float, 4>{0.0f, 1.0f, 0.0f, 1.0f}));
    CHECK(material.GetParameter("Gloss", value) && value[0] == 0.25f && value[1] == 0.75f);
    CHECK(material.GetParameter("Extra", value) && value[0] == 3.0f);

    // A broken edit: error text names the surface, the error surface is drawn, parameters stay.
    dir.Write("Params.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o)\n{\n    o.Albedo = Tnt.rgb;\n}\n");
    CHECK(!material.Compile(game.GetDevice(), game.GetContext()));
    CHECK(material.HasErrors() && material.GetErrors().find("Params.hlsl(3,") != std::string::npos);
    CHECK(material.IsCompiled() && !material.WritesGBuffer());
    CHECK(material.DependsOn(surface));
    CHECK(material.GetParameter("Tint", value) && value[1] == 1.0f);

    dir.Write("Params.hlsl", edited);
    CHECK(material.Compile(game.GetDevice(), game.GetContext()) && !material.HasErrors() && material.WritesGBuffer());
    CHECK(material.GetParameter("Tint", value) && value[1] == 1.0f);

    // Render state: compile-time options require a recompile, shadow casting does not.
    MaterialRenderState state = material.GetRenderState();
    state.bCastShadows = false;
    material.SetRenderState(state);
    CHECK(!material.NeedsCompile());
    state.Blend = MaterialBlendMode::Translucent;
    material.SetRenderState(state);
    CHECK(material.NeedsCompile());
    CHECK(material.Compile(game.GetDevice(), game.GetContext()) && !material.WritesGBuffer());

    // The parameter buffer must use the slot the engine binds.
    const std::string wrongSlot = dir.Write("WrongSlot.hlsl",
        "cbuffer MaterialParams : register(b3) { float4 Tint; };\n"
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tint.rgb; }\n");
    Material misplaced(wrongSlot);
    CHECK(!misplaced.Compile(game.GetDevice(), game.GetContext()));
    CHECK(misplaced.GetErrors().find("register(b1)") != std::string::npos);
}

// 1x1 uncompressed 32-bit TGA, top-left origin.
std::string WriteTga(const TempShaderDir& dir, const std::string& name, uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t header[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 0x28};
    const uint8_t pixel[4] = {b, g, r, 255};
    std::string bytes(reinterpret_cast<const char*>(header), sizeof(header));
    bytes.append(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    return dir.Write(name, bytes);
}

void MaterialOffscreenRendering()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    game.SetCamera({0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    auto* plane = new MeshComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    plane->SetMesh(MeshPrimitives::Plane(2.0f, 2.0f));
    CHECK(game.RegisterComponent("plane", plane) == RegisterResult::Ok);

    TempShaderDir dir;
    MaterialRenderState unlit;
    unlit.bUnlit = true; // exact colours: no lights involved
    Material material(dir.Write("Params.hlsl", ParamsSurface), unlit);
    CHECK(material.Compile(game.GetDevice(), game.GetContext()));
    CHECK(material.SetParameter("Transform", {}) == false);

    auto draw = [&](Material& m)
    {
        return RenderOffscreen(game, *plane, D3D11_CULL_BACK, [&](ID3D11DeviceContext* context)
        {
            plane->SetVertexShader(m.GetVertexShader());
            plane->SetPixelShader(m.GetPixelShader(false));
            m.Bind(context);
        })[32 * 64 + 32];
    };

    // The unused matrix is zero: every uv maps to (0, 0) of a 1x1 texture anyway.
    CHECK(draw(material) == 0xff0000ffu);  // default Tint red x white texture (RGBA8 little endian)
    CHECK(material.SetParameter("Tint", {0.0f, 1.0f, 0.0f, 1.0f}));
    CHECK(draw(material) == 0xff00ff00u);  // updated parameter reaches the GPU
    CHECK(material.SetParameter("Tint", {1.0f, 1.0f, 1.0f, 1.0f}));
    CHECK(material.SetTexture("BaseMap", WriteTga(dir, "Blue.tga", 0, 0, 255)));
    CHECK(draw(material) == 0xffff0000u);  // texture at t6 through the material sampler
    CHECK(!material.SetTexture("BaseMap", (dir.Root / "Missing.tga").generic_u8string()));
    CHECK(material.SetTexture("BaseMap", ""));
    CHECK(draw(material) == 0xffffffffu);  // back to white

    // A broken material draws the unlit magenta/black checker.
    Material broken(dir.Write("Broken.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tnt; }\n"));
    CHECK(!broken.Compile(game.GetDevice(), game.GetContext()));
    const uint32_t errorPixel = draw(broken);
    CHECK(errorPixel == 0xffff00ffu || errorPixel == 0xff140014u);
}

std::string ReadAll(const std::string& path)
{
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void MaterialAssetFormat()
{
    TempShaderDir dir;
    const std::string mat = dir.Write("Gold.mat", R"({
  "version": 1,
  "shader": "Gold.hlsl",
  "editor": { "previewMesh": "sphere" },
  "render": { "blend": "masked", "alphaClip": 0.3, "lighting": "unlit", "twoSided": true, "castShadows": false, "sortBias": 2 },
  "params": { "Tint": [1.0, 0.78, 0.34, 1.0], "Gloss": 0.8 },
  "textures": { "BaseMap": "Textures/gold.tga", "MaskMap": null }
})");
    MaterialAsset asset;
    std::string error;
    CHECK(MaterialAsset::Load(mat, asset, error));
    CHECK(asset.Shader == "Gold.hlsl");
    CHECK(asset.Render.Blend == MaterialBlendMode::Masked && asset.Render.bUnlit && asset.Render.bTwoSided && !asset.Render.bCastShadows);
    Near(asset.Render.AlphaClip, 0.3f);
    CHECK(asset.Params.size() == 2 && asset.Params[0].first == "Tint" && asset.Params[1].first == "Gloss");
    CHECK(asset.Params[0].second.size() == 4 && asset.Params[1].second == std::vector<float>{0.8f});
    CHECK(asset.Textures.size() == 2 && asset.Textures[0].second == "Textures/gold.tga" && asset.Textures[1].second.empty());

    MaterialAsset minimal;
    CHECK(MaterialAsset::Load(dir.Write("Min.mat", "\xef\xbb\xbf{ \"shader\": \"A.hlsl\" }"), minimal, error)); // BOM is accepted
    CHECK(minimal.Render == MaterialRenderState{} && minimal.Params.empty() && minimal.Textures.empty());

    // Every rejected file gets a message naming the file and the field.
    const std::pair<const char*, const char*> invalid[] = {
        {"{ not json", "invalid JSON"},
        {"[]", "root must be"},
        {"{}", "\"shader\""},
        {R"({"shader": "a", "version": 2})", "unsupported version"},
        {R"({"shader": "a", "render": {"blend": "glass"}})", "render.blend"},
        {R"({"shader": "a", "render": {"alphaClip": 2}})", "render.alphaClip"},
        {R"({"shader": "a", "render": {"lighting": "dark"}})", "render.lighting"},
        {R"({"shader": "a", "render": {"twoSided": "yes"}})", "render.twoSided"},
        {R"({"shader": "a", "params": {"X": [1, 2, 3, 4, 5]}})", "params.X"},
        {R"({"shader": "a", "params": {"X": [1, "2"]}})", "params.X"},
        {R"({"shader": "a", "textures": {"T": 5}})", "textures.T"},
    };
    for (const auto& file : invalid)
    {
        MaterialAsset rejected;
        error.clear();
        CHECK(!MaterialAsset::Load(dir.Write("Bad.mat", file.first), rejected, error));
        CHECK(error.find("Bad.mat") != std::string::npos && error.find(file.second) != std::string::npos);
    }
    CHECK(!MaterialAsset::Load((dir.Root / "Missing.mat").generic_u8string(), asset, error) && error.find("cannot read") != std::string::npos);

    // Saving keeps unknown fields and their order, numbers stay short.
    asset.Params[1].second = {0.3f};
    asset.Textures[1].second = "Textures/mask.tga";
    CHECK(asset.Save(mat, error));
    const std::string saved = ReadAll(mat);
    CHECK(saved.find("\"previewMesh\": \"sphere\"") != std::string::npos && saved.find("\"sortBias\": 2") != std::string::npos);
    CHECK(saved.find("\"shader\"") < saved.find("\"editor\"") && saved.find("\"editor\"") < saved.find("\"render\""));
    CHECK(saved.find("0.3") != std::string::npos && saved.find("0.300000") == std::string::npos);
    CHECK(saved.find("0.78") != std::string::npos && saved.find("0.779999") == std::string::npos);
    MaterialAsset reloaded;
    CHECK(MaterialAsset::Load(mat, reloaded, error));
    CHECK(reloaded.Render == asset.Render && reloaded.Params == asset.Params && reloaded.Textures == asset.Textures);

    const std::string resolved = MaterialAsset::ResolvePath(mat, "Textures/gold.tga");
    CHECK(resolved == (dir.Root / "Textures" / "gold.tga").generic_u8string());
    CHECK(MaterialAsset::MakeRelativePath(mat, resolved) == "Textures/gold.tga");
    CHECK(MaterialAsset::MakeRelativePath(mat, (dir.Root.parent_path() / "Shared.tga").generic_u8string()) == "../Shared.tga");
    CHECK(MaterialAsset::ResolvePath(mat, resolved) == resolved);
}

void GameMaterialCache()
{
    TestGame game;
    TempShaderDir dir;
    std::filesystem::create_directories(dir.Root / "Textures");
    const std::string surface = dir.Write("Params.hlsl", ParamsSurface);
    WriteTga(dir, "Textures/Blue.tga", 0, 0, 255);
    const char* matText = R"({
  "shader": "Params.hlsl",
  "params": { "Tint": [0, 1, 0, 1], "Gloss": 0.8, "Stale": 1 },
  "textures": { "BaseMap": "Textures/Blue.tga" }
})";
    const std::string mat = dir.Write("Green.mat", matText);

    std::shared_ptr<Material> material = game.LoadMaterial(mat);
    CHECK(material && material->IsCompiled() && !material->HasErrors());
    std::array<float, 4> value{};
    CHECK(material->GetParameter("Gloss", value) && value[0] == 0.8f);
    CHECK(material->GetParameter("Tint", value) && value[1] == 1.0f && value[0] == 0.0f);
    CHECK(material->GetTextures()[0].Path == MaterialAsset::ResolvePath(mat, "Textures/Blue.tga") && material->GetTextures()[0].View);
    std::string backslashes = mat;
    std::replace(backslashes.begin(), backslashes.end(), '/', '\\');
    CHECK(game.LoadMaterial(backslashes) == material); // one material per file

    // Editing the .mat file and invalidating it updates the same object.
    std::string edited = matText;
    edited.replace(edited.find("0.8"), 3, "0.3");
    dir.Write("Green.mat", edited);
    CHECK(game.InvalidateShaderFile(mat));
    CHECK(game.LoadMaterial(mat) == material);
    CHECK(material->GetParameter("Gloss", value) && value[0] == 0.3f);

    // Shader edits recompile materials that read the file; values survive a broken intermediate state.
    dir.Write("Params.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tnt; }\n");
    CHECK(game.InvalidateShaderFile(surface));
    CHECK(material->HasErrors());
    dir.Write("Params.hlsl", ParamsSurface);
    CHECK(game.InvalidateShaderFile(surface) && !material->HasErrors());
    CHECK(material->GetParameter("Gloss", value) && value[0] == 0.3f);

    // A broken .mat keeps the previous state; an unusable file gives no material and a reason.
    dir.Write("Green.mat", R"({ "shader": "Params.hlsl", "render": { "blend": "glass" } })");
    std::string error;
    CHECK(!game.ReloadMaterial(mat, &error) && error.find("render.blend") != std::string::npos);
    CHECK(material->GetParameter("Gloss", value) && value[0] == 0.3f);
    CHECK(game.LoadMaterial(dir.Write("Other.mat", "{}"), &error) == nullptr && error.find("\"shader\"") != std::string::npos);

    std::shared_ptr<Material> fallback = game.GetDefaultMaterial();
    CHECK(fallback && !fallback->HasErrors() && fallback->WritesGBuffer() && game.GetDefaultMaterial() == fallback);

    // Saving the live material: its values, the author's paths and the parameter the shader dropped.
    dir.Write("Green.mat", edited);
    MaterialAsset base;
    CHECK(MaterialAsset::Load(mat, base, error));
    CHECK(material->SetParameter("Gloss", {0.5f, 0, 0, 0}));
    CHECK(MaterialAsset::FromMaterial(*material, mat, base).Save(mat, error));
    MaterialAsset saved;
    CHECK(MaterialAsset::Load(mat, saved, error));
    CHECK(saved.Shader == "Params.hlsl");
    CHECK(saved.Textures.size() == 1 && saved.Textures[0].second == "Textures/Blue.tga");
    const auto gloss = std::find_if(saved.Params.begin(), saved.Params.end(), [](const auto& p) { return p.first == "Gloss"; });
    CHECK(gloss != saved.Params.end() && gloss->second == std::vector<float>{0.5f});
    CHECK(std::any_of(saved.Params.begin(), saved.Params.end(), [](const auto& p) { return p.first == "Stale"; }));
    CHECK(std::any_of(saved.Params.begin(), saved.Params.end(), [](const auto& p) { return p.first == "UVScale"; }));
}

void MaterialComponentIntegration()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    game.SetCamera({0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    TempShaderDir dir;
    auto material = std::make_shared<Material>(dir.Write("Params.hlsl", ParamsSurface));

    // Only Mesh-format components accept a material.
    GameComponent primitive;
    CHECK(!primitive.SetMaterial(material) && primitive.GetMaterial() == nullptr);

    auto* plane = new MeshComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    plane->SetMesh(MeshPrimitives::Plane(2.0f, 2.0f));
    CHECK(game.RegisterComponent("plane", plane) == RegisterResult::Ok);
    CHECK(game.BindComponentShaders(plane, ShaderCompileVariant::Default));
    // A mesh registered without a shader pair (as by a scene) draws with the default material.
    const uint32_t defaultPixel = RenderOffscreen(game, *plane, D3D11_CULL_BACK)[32 * 64 + 32];
    CHECK(defaultPixel != 0u);
    CHECK(plane->SetMaterial(material));

    // Opaque lit: compiled on first use, drawn by the G-buffer pass, casts shadows.
    CHECK(material->NeedsCompile());
    CHECK(game.SupportsDeferredGeometry(plane) && !material->NeedsCompile());
    CHECK(game.BindComponentShaders(plane, ShaderCompileVariant::DeferredGBuffer));
    CHECK(!plane->HasOpacity() && plane->CastsShadow());

    // Render state changes reach the passes without any call on the component.
    MaterialRenderState state;
    state.bCastShadows = false;
    material->SetRenderState(state);
    CHECK(!plane->CastsShadow() && !plane->HasOpacity());
    state.Blend = MaterialBlendMode::Translucent;
    material->SetRenderState(state);
    CHECK(plane->HasOpacity() && !game.SupportsDeferredGeometry(plane));
    state = MaterialRenderState{};
    state.bUnlit = true;
    material->SetRenderState(state);
    CHECK(!game.BindComponentShaders(plane, ShaderCompileVariant::DeferredGBuffer)); // forward-only
    CHECK(game.BindComponentShaders(plane, ShaderCompileVariant::Default));
    CHECK(!game.SupportsDeferredGeometry(plane) && !plane->HasOpacity() && plane->CastsShadow());

    // Drawing through the regular path binds the material's parameters.
    CHECK(material->SetParameter("Tint", {0.0f, 0.0f, 1.0f, 1.0f}));
    CHECK(RenderOffscreen(game, *plane, D3D11_CULL_BACK)[32 * 64 + 32] == 0xffff0000u);

    // Without the material the default one draws it again.
    CHECK(plane->SetMaterial(nullptr));
    CHECK(game.BindComponentShaders(plane, ShaderCompileVariant::Default));
    CHECK(game.SupportsDeferredGeometry(plane));
    CHECK(RenderOffscreen(game, *plane, D3D11_CULL_BACK)[32 * 64 + 32] == defaultPixel);
}

// Shadow pass of one component into a 64x64 depth map seen from above; returns the depth values.
// cascadeIndex -1 is the baked static map (every copy of an instanced component, no cascade culling).
std::vector<float> RenderShadowDepth(TestGame& game, GameComponent& component, int cascadeIndex = 0)
{
    constexpr UINT Size = 64;
    ID3D11Device* device = game.GetDevice();
    ID3D11DeviceContext* context = game.GetContext();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = Size; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    CheckHR(device->CreateTexture2D(&desc, nullptr, depth.GetAddressOf()));
    CheckHR(device->CreateDepthStencilView(depth.Get(), nullptr, dsv.GetAddressOf()));

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID; rasterDesc.CullMode = D3D11_CULL_NONE; rasterDesc.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> raster;
    CheckHR(device->CreateRasterizerState(&rasterDesc, raster.GetAddressOf()));

    context->ClearState();
    context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    context->OMSetRenderTargets(0, nullptr, dsv.Get());
    D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(Size), static_cast<float>(Size), 0.0f, 1.0f};
    context->RSSetViewports(1, &viewport);
    context->RSSetState(raster.Get());

    const XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 3, 0, 1), XMVectorSet(0, 0, 0, 1), XMVectorSet(0, 0, 1, 0));
    const XMMATRIX projection = XMMatrixOrthographicLH(3.0f, 3.0f, 0.1f, 10.0f);
    ShadowPassContext pass;
    XMStoreFloat4x4(&pass.LightViewProjection, XMMatrixTranspose(view * projection));
    pass.VertexShader = game.GetVertexShader(EnginePaths::Shader("ShadowDepth.hlsl").generic_u8string());
    // As in Game::InitShadowResources: the mesh layout of the shadow pass is built against the
    // position-only signature of the depth shader.
    const D3D11_INPUT_ELEMENT_DESC meshElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    ShaderCompiler::CompileResult depthShader = ShaderCompiler::Compile(
        EnginePaths::Shader("ShadowDepth.hlsl").generic_u8string(), "VSMain", "vs_5_0");
    CHECK(depthShader.Succeeded());
    ComPtr<ID3D11InputLayout> shadowLayout;
    CheckHR(device->CreateInputLayout(meshElements, 3, depthShader.Blob->GetBufferPointer(), depthShader.Blob->GetBufferSize(),
                                      shadowLayout.GetAddressOf()));
    pass.MeshLayout = shadowLayout.Get();
    ShaderCompiler::CompileResult instancedShader = ShaderCompiler::Compile(
        EnginePaths::Shader("ShadowDepth.hlsl").generic_u8string(), "VSMainInstanced", "vs_5_0");
    CHECK(instancedShader.Succeeded());
    ComPtr<ID3D11VertexShader> instancedVertexShader;
    CheckHR(device->CreateVertexShader(instancedShader.Blob->GetBufferPointer(), instancedShader.Blob->GetBufferSize(),
                                       nullptr, instancedVertexShader.GetAddressOf()));
    pass.InstancedVertexShader = instancedVertexShader.Get();
    pass.CascadeIndex = cascadeIndex;
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(ShadowPassBufferData);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> shadowBuffer;
    CheckHR(device->CreateBuffer(&bufferDesc, nullptr, shadowBuffer.GetAddressOf()));
    pass.ConstantBuffer = shadowBuffer.Get();
    component.Update();
    component.RenderShadow(context, pass);

    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    CheckHR(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()));
    context->CopyResource(staging.Get(), depth.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CheckHR(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<float> values(Size * Size);
    for (UINT y = 0; y < Size; ++y)
    {
        std::memcpy(&values[y * Size], static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch, Size * sizeof(float));
    }
    context->Unmap(staging.Get(), 0);
    return values;
}

void MaskedMaterialShadows()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    TempShaderDir dir;
    // Only the left half (u < 0.5, -X) is solid.
    auto material = std::make_shared<Material>(dir.Write("HalfMask.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Alpha = i.UV.x < 0.5f ? 1.0f : 0.0f; }\n"));
    auto* plane = new MeshComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    plane->SetMesh(MeshPrimitives::Plane(2.0f, 2.0f));
    CHECK(game.RegisterComponent("plane", plane) == RegisterResult::Ok);
    CHECK(plane->SetMaterial(material));

    // Opaque: the engine's depth shader, the whole plane casts.
    CHECK(material->Compile(game.GetDevice(), game.GetContext()) && !material->HasShadowShaders());
    std::vector<float> depth = RenderShadowDepth(game, *plane);
    const size_t left = 32 * 64 + 16, right = 32 * 64 + 48, outside = 32 * 64 + 1;
    CHECK(depth[left] < 1.0f && depth[right] < 1.0f && depth[outside] == 1.0f);

    // Masked: the material's own shadow shaders, compiled on the first shadow pass, clip the right half.
    MaterialRenderState state;
    state.Blend = MaterialBlendMode::Masked;
    material->SetRenderState(state);
    depth = RenderShadowDepth(game, *plane);
    CHECK(material->HasShadowShaders());
    CHECK(depth[left] < 1.0f && depth[right] == 1.0f);
    Near(depth[left], RenderShadowDepth(game, *plane)[left]); // same depth as the opaque path

    // The threshold of the material is used: clip discards alpha below it, so alpha 1 survives a threshold of 1.
    state.AlphaClip = 1.0f;
    material->SetRenderState(state);
    CHECK(RenderShadowDepth(game, *plane)[left] < 1.0f);
    // Back to opaque: the shared depth shader again, the whole plane casts.
    state.Blend = MaterialBlendMode::Opaque;
    material->SetRenderState(state);
    CHECK(RenderShadowDepth(game, *plane)[right] < 1.0f && !material->HasShadowShaders());
}

std::vector<uint32_t> ReadTexture(TestGame& game, ID3D11ShaderResourceView* view, UINT& width)
{
    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    CheckHR(resource.As(&texture));
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    CheckHR(game.GetDevice()->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()));
    game.GetContext()->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    CheckHR(game.GetContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint32_t> pixels(desc.Width * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
    {
        std::memcpy(&pixels[y * desc.Width], static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch, desc.Width * sizeof(uint32_t));
    }
    game.GetContext()->Unmap(staging.Get(), 0);
    width = desc.Width;
    return pixels;
}

void InstancedMaterials()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    game.SetCamera({0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    TempShaderDir dir;
    MaterialRenderState unlit;
    unlit.bUnlit = true; // exact colours
    // Colour of the copy plus its custom data.
    auto material = std::make_shared<Material>(dir.Write("PerCopy.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = i.Color.rgb + i.InstanceData; }\n"), unlit);

    // InstancedMeshComponent: a red copy left of the centre, a black copy with green data on the right.
    auto* units = new InstancedMeshComponent();
    CHECK(units->SetLod(0, MeshPrimitives::Plane(0.8f, 0.8f)));
    MeshInstance left;
    left.Position = {-0.6f, 0.0f, 0.0f};
    left.Color = {1.0f, 0.0f, 0.0f, 1.0f};
    MeshInstance right;
    right.Position = {0.6f, 0.0f, 0.0f};
    right.Color = {0.0f, 0.0f, 0.0f, 1.0f};
    right.CustomData = {0.0f, 1.0f, 0.0f};
    units->AddInstance(left);
    units->AddInstance(right);
    CHECK(game.RegisterComponent("units", units) == RegisterResult::Ok);
    CHECK(units->SetMaterial(material));

    // The instanced set is chosen and compiled on first use; 18.5 px per unit at this camera.
    std::vector<uint32_t> pixels = RenderOffscreen(game, *units, D3D11_CULL_BACK);
    CHECK(material->IsVariantRequired(MaterialVariant::Instanced) && material->GetVertexShader(MaterialVariant::Instanced) != nullptr);
    CHECK(pixels[32 * 64 + 21] == 0xff0000ffu); // red copy
    CHECK(pixels[32 * 64 + 43] == 0xff00ff00u); // green from custom data
    CHECK(pixels[32 * 64 + 32] == 0u);          // gap between the copies

    // Copies made with CreateInstance on a MeshComponent use the same instanced set.
    auto* copies = new MeshComponent(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(1.0f));
    copies->SetMesh(MeshPrimitives::Plane(0.8f, 0.8f));
    CHECK(game.RegisterComponent("copies", copies) == RegisterResult::Ok);
    copies->CreateInstance({-0.6f, 0.0f, 0.0f}, glm::vec3(0.0f), glm::vec3(1.0f), {1.0f, 0.0f, 0.0f, 1.0f});
    copies->CreateInstance({0.6f, 0.0f, 0.0f}, glm::vec3(0.0f), glm::vec3(1.0f), {0.0f, 0.0f, 1.0f, 1.0f});
    CHECK(copies->SetMaterial(material) && copies->GetMaterialVariant() == MaterialVariant::Instanced);
    pixels = RenderOffscreen(game, *copies, D3D11_CULL_BACK);
    CHECK(pixels[32 * 64 + 21] == 0xff0000ffu && pixels[32 * 64 + 43] == 0xffff0000u && pixels[32 * 64 + 32] == 0u);

    // Masked instanced shadows: the clip of every copy reaches the baked map (21.3 px per unit).
    auto masked = std::make_shared<Material>(dir.Write("HalfMask.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Alpha = i.UV.x < 0.5f ? 1.0f : 0.0f; }\n"));
    MaterialRenderState maskedState;
    maskedState.Blend = MaterialBlendMode::Masked;
    masked->SetRenderState(maskedState);
    CHECK(units->SetMaterial(masked));
    std::vector<float> depth = RenderShadowDepth(game, *units, -1);
    CHECK(masked->GetShadowPixelShader(MaterialVariant::Instanced) != nullptr);
    CHECK(depth[32 * 64 + 15] < 1.0f && depth[32 * 64 + 24] == 1.0f);  // left copy: solid | clipped
    CHECK(depth[32 * 64 + 40] < 1.0f && depth[32 * 64 + 49] == 1.0f);  // right copy
    std::vector<float> plainDepth = RenderShadowDepth(game, *copies, -1); // opaque material: whole copies
    CHECK(plainDepth[32 * 64 + 24] < 1.0f && plainDepth[32 * 64 + 49] < 1.0f);
}

// One bone that the only clip holds 0.6 to the right (+X) of the bind pose; a 0.8 quad facing +Y on it.
SkeletalModel ShiftedQuadModel()
{
    SkeletalModel model;
    Bone root;
    root.Name = "Root";
    XMStoreFloat4x4(&root.LocalTransform, XMMatrixIdentity());
    XMStoreFloat4x4(&root.InverseBindPose, XMMatrixIdentity());
    model.ModelSkeleton.Bones.push_back(root);

    AnimationClip clip;
    clip.Name = "Shift";
    clip.Duration = 1.0f;
    NodeAnimationChannel channel;
    channel.NodeName = "Root";
    channel.BoneIndex = 0;
    channel.PositionKeys = {{0.0f, {0.6f, 0.0f, 0.0f}}, {1.0f, {0.6f, 0.0f, 0.0f}}};
    clip.Channels.push_back(channel);
    model.Clips.push_back(clip);

    SkeletalMesh mesh;
    const MeshLodData quad = MeshPrimitives::Plane(0.8f, 0.8f);
    for (const MeshVertex& source : quad.Vertices)
    {
        SkinnedVertex vertex;
        vertex.Position = {source.Position.x, source.Position.y, source.Position.z};
        vertex.Normal = {source.Normal.x, source.Normal.y, source.Normal.z};
        vertex.TexCoord = {source.TexCoord.x, source.TexCoord.y};
        vertex.BoneWeights[0] = 1.0f;
        mesh.Vertices.push_back(vertex);
    }
    mesh.Indices = quad.Indices;
    model.TotalIndexCount = static_cast<int>(mesh.Indices.size());
    model.BoundingRadius = 1.0f;
    model.Meshes.push_back(mesh);
    return model;
}

void SkinnedMaterials()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    game.SetCamera({0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    TempShaderDir dir;
    MaterialRenderState unlit;
    unlit.bUnlit = true;
    auto material = std::make_shared<Material>(dir.Write("Red.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = float3(1, 0, 0); }\n"), unlit);
    const size_t centre = 32 * 64 + 32, right = 32 * 64 + 43;

    // One animated object: the bone palette moves the quad off the centre.
    auto* skeletal = new SkeletalMeshComponent();
    CHECK(skeletal->SetModel(std::make_shared<SkeletalModel>(ShiftedQuadModel())));
    CHECK(game.RegisterComponent("skeletal", skeletal) == RegisterResult::Ok);
    CHECK(skeletal->SetMaterial(material) && skeletal->GetMaterialVariant() == MaterialVariant::Skinned);
    std::vector<uint32_t> pixels = RenderOffscreen(game, *skeletal, D3D11_CULL_NONE);
    CHECK(material->GetVertexShader(MaterialVariant::Skinned) != nullptr);
    CHECK(pixels[right] == 0xff0000ffu && pixels[centre] == 0u);

    // A crowd: the same pose baked into the animation texture, one copy at the origin.
    auto* crowd = new InstancedMeshComponent();
    CHECK(crowd->SetAnimatedModel(game.GetDevice(), ShiftedQuadModel()));
    crowd->AddInstance(MeshInstance{});
    CHECK(game.RegisterComponent("crowd", crowd) == RegisterResult::Ok);
    CHECK(crowd->SetMaterial(material));
    CHECK(crowd->GetMaterialVariant() == (MaterialVariant::Instanced | MaterialVariant::Skinned));
    pixels = RenderOffscreen(game, *crowd, D3D11_CULL_NONE);
    CHECK(pixels[right] == 0xff0000ffu && pixels[centre] == 0u);
}

void ImpostorBakedThroughMaterial()
{
    TestGame game;
    CHECK(game.CreateInputLayouts());
    TempShaderDir dir;
    Material material(dir.Write("Bake.hlsl",
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = float3(0.25f, 0.5f, 1.0f); }\n"));
    CHECK(material.Compile(game.GetDevice(), game.GetContext()));

    ImpostorAtlasSettings settings;
    settings.YawCount = 4;
    settings.PitchCount = 1;
    settings.TileSize = 32;
    const MeshLodData sphere = MeshPrimitives::Sphere(0.5f);
    auto centreOfTile = [&](ImpostorAtlas& atlas)
    {
        UINT width = 0;
        const std::vector<uint32_t> albedo = ReadTexture(game, atlas.GetAlbedoSRV(), width);
        return albedo[16 * width + 16];
    };

    ImpostorAtlas plain;
    CHECK(plain.Build(game, sphere, nullptr, glm::vec4(1.0f), settings));
    CHECK(centreOfTile(plain) == 0xffffffffu);

    ImpostorAtlas baked;
    CHECK(baked.Build(game, sphere, nullptr, glm::vec4(1.0f), settings, &material));
    CHECK(centreOfTile(baked) == 0xffff8040u); // (0.25, 0.5, 1.0), full coverage

    // Sprites of a one-sided material are drawn although the pass culls back faces for its meshes.
    // Without a window every copy is below the sprite threshold. The camera is tilted: straight
    // down, the sprite shader has no view direction around the vertical to pick a tile from.
    game.SetCamera({0.0f, 3.0f, -1.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    auto* units = new InstancedMeshComponent();
    CHECK(units->SetLod(0, sphere));
    units->SetLodScreenHeights({10.0f});
    units->SetImpostorEnabled(true, settings);
    units->AddInstance(MeshInstance{});
    auto unitMaterial = std::make_shared<Material>(dir.Write("Bake2.hlsl",
        "cbuffer MaterialParams : register(b1) { float4 Tint = float4(0.25f, 0.5f, 1.0f, 1.0f); };\n"
        "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tint.rgb; }\n"));
    CHECK(units->SetMaterial(unitMaterial));
    CHECK(game.RegisterComponent("units", units) == RegisterResult::Ok);
    CHECK(units->HasImpostor());
    const uint32_t sprite = RenderOffscreen(game, *units, D3D11_CULL_BACK)[32 * 64 + 32];
    CHECK(sprite != 0u);
    CHECK(units->GetDrawnImpostors() == 1 && units->GetDrawnInstancesForLod(0) == 0);

    // A parameter change reaches the sprites on the next tick: the atlas is rebaked.
    CHECK(unitMaterial->SetParameter("Tint", {1.0f, 0.0f, 0.0f, 1.0f}));
    units->Tick(0.0f);
    CHECK(RenderOffscreen(game, *units, D3D11_CULL_BACK)[32 * 64 + 32] != sprite);

    // A .mat reloaded into the same object never repeats its revision, so its atlases are rebaked too.
    std::shared_ptr<Material> loaded = game.LoadMaterial(dir.Write("Units.mat", R"({ "shader": "Bake2.hlsl" })"));
    const uint64_t revision = loaded ? loaded->GetRevision() : 0;
    CHECK(loaded && game.ReloadMaterial((dir.Root / "Units.mat").generic_u8string()) && loaded->GetRevision() != revision);

    // A material with errors cannot bake: the plain bake is used instead.
    Material broken(dir.Write("Broken.hlsl", "void Surface(MaterialInput i, inout MaterialOutput o) { o.Albedo = Tnt; }\n"));
    CHECK(!broken.Compile(game.GetDevice(), game.GetContext()));
    ImpostorAtlas fallback;
    CHECK(fallback.Build(game, sphere, nullptr, glm::vec4(1.0f), settings, &broken));
    CHECK(centreOfTile(fallback) == 0xffffffffu);
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

    // Shader include chain via VFS: a.hlsl -> sub/b.hlsli -> ../c.hlsli
    auto repoRoot = EnginePaths::ExecutableDirectory();
    for (int i = 0; i < 8 && !repoRoot.empty(); ++i)
    {
        if (std::filesystem::exists(repoRoot / ".git") || std::filesystem::exists(repoRoot / "Engine" / "Engine.props"))
            break;
        repoRoot = repoRoot.parent_path();
    }
    const auto fixtureRoot = (repoRoot / "Engine" / "tests" / "ShaderIncludeFixture").lexically_normal();
    std::filesystem::remove_all(fixtureRoot, ec);
    std::filesystem::create_directories(fixtureRoot / "sub", ec);

    struct FixtureGuard
    {
        std::filesystem::path root;
        FixtureGuard(const std::filesystem::path& r) : root(r)
        {
            VfsRead::GetVFS().Mount("/TestFixture", root);
        }
        ~FixtureGuard()
        {
            VfsRead::GetVFS().Unmount("/TestFixture");
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    } fixtureGuard(fixtureRoot);

    const auto aHlsl = fixtureRoot / "a.hlsl";
    const auto bHlsli = fixtureRoot / "sub" / "b.hlsli";
    const auto cHlsli = fixtureRoot / "c.hlsli";
    const auto badHlsl = fixtureRoot / "bad.hlsl";

    {
        std::ofstream out(cHlsli);
        out << "#define VALUE_C 10.0f\n";
    }
    {
        std::ofstream out(bHlsli);
        out << "#include \"../c.hlsli\"\n#define VALUE_B (VALUE_C + 5.0f)\n";
    }
    {
        std::ofstream out(aHlsl);
        out << "#include \"sub/b.hlsli\"\n"
            << "float4 main() : SV_Target\n"
            << "{\n"
            << "    return float4(VALUE_B, 0.0f, 0.0f, 1.0f);\n"
            << "}\n";
    }
    {
        std::ofstream out(badHlsl);
        out << "#include \"missing.hlsli\"\n"
            << "float4 main() : SV_Target\n"
            << "{\n"
            << "    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
            << "}\n";
    }

    // 1. Successful compilation of chain a.hlsl -> sub/b.hlsli -> ../c.hlsli
    ComPtr<ID3DBlob> shaderBlob;
    std::string compileError;
    const HRESULT hr = ShaderCompiler::CompileFromFile(aHlsl.wstring(), nullptr, "main", "ps_5_0", shaderBlob.GetAddressOf(), &compileError);
    CHECK(SUCCEEDED(hr));
    CHECK(shaderBlob != nullptr);
    CHECK(shaderBlob->GetBufferSize() > 0);
    shaderBlob.Reset();

    // 2. Missing include produces error and log
    std::stringstream logCapture;
    auto* prevCoutBuf = std::cout.rdbuf(logCapture.rdbuf());
    const HRESULT badHr = ShaderCompiler::CompileFromFile(badHlsl.wstring(), nullptr, "main", "ps_5_0", shaderBlob.GetAddressOf(), &compileError);
    std::cout.rdbuf(prevCoutBuf);

    CHECK(FAILED(badHr));
    CHECK(shaderBlob == nullptr);
    CHECK(!compileError.empty());
    const std::string capturedLog = logCapture.str();
    CHECK(capturedLog.find("[ShaderCompiler] Include file not found in VFS") != std::string::npos);
    CHECK(capturedLog.find("missing.hlsli") != std::string::npos);

    // fixtureGuard automatically unmounts /TestFixture and removes fixtureRoot on exit
}

/** A sphere of quads: enough vertices to simplify, and every normal must point away from the centre. */
MeshLodData MakeTestSphere(int segments, float radius)
{
    MeshLodData mesh;
    for (int ring = 0; ring <= segments; ++ring)
    {
        const float v = static_cast<float>(ring) / static_cast<float>(segments);
        const float phi = v * 3.14159265f;
        for (int slice = 0; slice <= segments; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(segments);
            const float theta = u * 6.2831853f;
            const float x = std::sin(phi) * std::cos(theta);
            const float y = std::cos(phi);
            const float z = std::sin(phi) * std::sin(theta);
            MeshVertex vertex;
            vertex.Position = XMFLOAT4(x * radius, y * radius + radius, z * radius, 1.0f);
            vertex.Normal = XMFLOAT4(x, y, z, 0.0f);
            vertex.TexCoord = XMFLOAT4(u, v, 0.0f, 0.0f);
            mesh.Vertices.push_back(vertex);
        }
    }
    const uint32_t stride = static_cast<uint32_t>(segments + 1);
    for (int ring = 0; ring < segments; ++ring)
    {
        for (int slice = 0; slice < segments; ++slice)
        {
            const uint32_t topLeft = static_cast<uint32_t>(ring) * stride + static_cast<uint32_t>(slice);
            const uint32_t bottomLeft = topLeft + stride;
            mesh.Indices.insert(mesh.Indices.end(),
                                {topLeft, bottomLeft, topLeft + 1, topLeft + 1, bottomLeft, bottomLeft + 1});
        }
    }
    return mesh;
}

void MeshSimplification()
{
    const MeshLodData source = MakeTestSphere(24, 2.0f);
    const int sourceTriangles = static_cast<int>(source.Indices.size() / 3);

    MeshSimplifySettings settings;
    settings.TargetRatio = 0.2f;
    settings.TargetError = 0.2f;
    MeshSimplifyResult result;
    const MeshLodData simplified = SimplifyMesh(source, settings, &result);

    CHECK(!simplified.Indices.empty());
    CHECK(simplified.Indices.size() % 3 == 0);
    CHECK(result.SourceTriangles == sourceTriangles);
    // Simplifying must actually remove triangles, and never add any.
    CHECK(result.ResultTriangles < sourceTriangles / 2);
    CHECK(result.ResultTriangles > 0);

    for (uint32_t index : simplified.Indices)
    {
        CHECK(index < simplified.Vertices.size());
    }

    // The rebuilt normals must still point outwards: a sign error here turns a model black.
    const MeshBounds bounds = ComputeMeshBounds(simplified);
    int outward = 0;
    for (const MeshVertex& vertex : simplified.Vertices)
    {
        const float dx = vertex.Position.x - bounds.Center.x;
        const float dy = vertex.Position.y - bounds.Center.y;
        const float dz = vertex.Position.z - bounds.Center.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        CHECK(length > 0.0001f);
        const float normalLength = std::sqrt(vertex.Normal.x * vertex.Normal.x +
                                             vertex.Normal.y * vertex.Normal.y +
                                             vertex.Normal.z * vertex.Normal.z);
        Near(normalLength, 1.0f);
        if ((vertex.Normal.x * dx + vertex.Normal.y * dy + vertex.Normal.z * dz) / length > 0.5f)
        {
            ++outward;
        }
    }
    CHECK(outward > static_cast<int>(simplified.Vertices.size()) * 9 / 10);

    // The engine draws with culling off, so nothing forces a model to wind every face the same
    // way - the procedural vehicles do not. Normals must come out of the model's own normals,
    // not out of the winding, or half the mesh ends up lit from the inside and renders black.
    MeshLodData mixedWinding = source;
    for (size_t triangle = 0; triangle * 3 + 2 < mixedWinding.Indices.size(); triangle += 2)
    {
        std::swap(mixedWinding.Indices[triangle * 3 + 1], mixedWinding.Indices[triangle * 3 + 2]);
    }
    const MeshLodData mixedResult = SimplifyMesh(mixedWinding, settings);
    CHECK(!mixedResult.Vertices.empty());

    const MeshBounds mixedBounds = ComputeMeshBounds(mixedResult);
    int mixedOutward = 0;
    for (const MeshVertex& vertex : mixedResult.Vertices)
    {
        const float dx = vertex.Position.x - mixedBounds.Center.x;
        const float dy = vertex.Position.y - mixedBounds.Center.y;
        const float dz = vertex.Position.z - mixedBounds.Center.z;
        if (vertex.Normal.x * dx + vertex.Normal.y * dy + vertex.Normal.z * dz > 0.0f)
        {
            ++mixedOutward;
        }
    }
    CHECK(mixedOutward > static_cast<int>(mixedResult.Vertices.size()) * 9 / 10);

    // A mesh that cannot be simplified must come back empty rather than broken.
    MeshLodData broken;
    broken.Vertices.resize(3);
    broken.Indices = {0, 1};
    CHECK(SimplifyMesh(broken, settings).Indices.empty());
}

void MeshValidationRules()
{
    const MeshLodData good = MakeTestSphere(8, 1.6f);
    const MeshValidationReport report = ValidateMesh(good);
    CHECK(report.IsUsable());
    CHECK(report.Triangles > 0);
    CHECK(report.bHasTexCoords);
    CHECK(!report.ToText("sphere").empty());

    // A model exported in centimetres and left hanging above the ground.
    MeshLodData wrongScale = good;
    for (MeshVertex& vertex : wrongScale.Vertices)
    {
        vertex.Position.x *= 100.0f;
        vertex.Position.y = vertex.Position.y * 100.0f + 50.0f;
        vertex.Position.z *= 100.0f;
    }
    const MeshValidationReport scaleReport = ValidateMesh(wrongScale);
    CHECK(!scaleReport.IsUsable());
    CHECK(scaleReport.Problems.size() >= 2); // size and pivot

    // Missing normals must be reported, not silently drawn black.
    MeshLodData noNormals = good;
    for (MeshVertex& vertex : noNormals.Vertices)
    {
        vertex.Normal = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    CHECK(!ValidateMesh(noNormals).IsUsable());

    MeshLodData empty;
    CHECK(!ValidateMesh(empty).IsUsable());
}
void SceneOwnershipTests();
void SceneHierarchyTests();
void SceneDeferredTests();
void SceneBatchTests();
void AuthoringTests();

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        // Scenario A MUST run first: depends on untouched global VFS before any BaseEngine lifecycle.
        {"vfs lifecycle A (read before engine)", VfsLifecycle_ScenarioA},
        {"scene ownership and IDs (no GPU)", SceneOwnershipTests},
        {"scene hierarchy and snapshots (no GPU)", SceneHierarchyTests},
        {"scene deferred mutations (no GPU)", SceneDeferredTests},
        {"atomic group transforms, references and deletion (no GPU)", SceneBatchTests},
        {"typed properties and transactional asset updates", AuthoringTests},
        {"registry ownership", RegistryOwnership}, {"registration failure ownership", RegistryFailures},
        {"transform hierarchy and cache", TransformHierarchy}, {"unregister hierarchy and instances", UnregisterHierarchyAndInstances},
        {"frame light snapshot", LightSnapshot}, {"forward/deferred shader variants", ShaderVariants},
        {"shader includes, overrides and dependencies", ShaderCompilerIncludes}, {"shader invalidation and errors", ShaderInvalidation},
        {"material template variants and reflection", MaterialTemplateVariants},
        {"mesh primitives: normals, uvs, winding", MeshPrimitivesGeometry}, {"mesh component offscreen rendering", MeshComponentRendering},
        {"material compilation, parameters, errors", MaterialCompilationAndParameters}, {"material offscreen rendering", MaterialOffscreenRendering},
        {".mat format: load, validate, save", MaterialAssetFormat}, {"game material cache and reload", GameMaterialCache},
        {"materials on components and passes", MaterialComponentIntegration},
        {"masked material shadows", MaskedMaterialShadows},
        {"instanced materials: copies, custom data, shadows", InstancedMaterials},
        {"impostor atlas baked through a material", ImpostorBakedThroughMaterial}, {"skinned materials: bone palette and crowd", SkinnedMaterials},
        {"particle GPU indexing", ParticleShaderIndexing}, {"particle component sorting modes", ParticleComponentModes},
        {"WM_QUIT exit code", QuitMessage}, {"virtual file system (VFS)", VirtualFileSystemTests},
        {"vfs lifecycle B (read after stop)", VfsLifecycle_ScenarioB},
        {"vfs lifecycle C (compiler after stop)", VfsLifecycle_ScenarioC},
        {"second run exits cleanly", VfsLifecycle_ScenarioD},
        {"VirtualFileSystem::Mount replaces same root", VfsLifecycle_ScenarioE},
        {"vfs mounts preserved after stop", VfsLifecycle_PreserveMountSet},
        {"vfs project content unmounted after stop", VfsLifecycle_ContentRootReverted},
        {"vfs premounted game restored after stop", VfsLifecycle_PreMountedGameReverted},
        {"mesh simplification", MeshSimplification}, {"mesh validation rules", MeshValidationRules},
        {"vfs service: resolve and toVirtual", VfsService_ResolveAndToVirtual},
        {"vfs service: exists and stat", VfsService_ExistsAndStat},
        {"vfs service: readFile and allocator", VfsService_ReadFileAndAllocator},
        {"vfs service: enumerate and find", VfsService_EnumerateAndFind},
        {"vfs service: callback reentrancy without deadlock", VfsService_CallbackReentrancyAndNoDeadlock},
        {"vfs service: mounts and unsupported stubs", VfsService_GetMountsAndStubs},
        {"vfs proxy lifecycle: sessions and rejection", VfsProxyLifecycle_SessionsAndRejection},
        {"vfs proxy lifecycle: read parity and allocator", VfsProxyLifecycle_ReadParityAndAllocator}
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
