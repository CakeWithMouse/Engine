#include <Engine/Resources/Paths.h>
#include <Engine/Resources/VirtualFileSystem.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Render/Material.h>
#include <Engine/Render/MaterialAsset.h>
#include <Engine/Render/MeshPrimitives.h>
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
    using Game::CreateInputLayouts;
    using Game::PumpMessages;
    void SetCamera(const XMFLOAT3& eye, const XMFLOAT3& at, const XMFLOAT3& up)
    {
        const XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&at), XMLoadFloat3(&up));
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1.0f, 0.1f, 100.0f);
        XMStoreFloat4x4(&CurrentFrame.viewMatrix, XMMatrixTranspose(view));
        XMStoreFloat4x4(&CurrentFrame.projectionMatrix, XMMatrixTranspose(projection));
        CurrentFrame.CameraPosition = XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);
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

    // Full reload (F6): an edit becomes visible without naming the file.
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
    ID3D11PixelShader* legacyShader = game.GetPixelShader(EnginePaths::Shader("RotatedFigure.hlsl").generic_u8string());
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

    // Without the material the shader pair is used again.
    CHECK(plane->SetMaterial(nullptr));
    CHECK(game.BindComponentShaders(plane, ShaderCompileVariant::Default));
    CHECK(game.SupportsDeferredGeometry(plane));
    CHECK(legacyShader != nullptr && legacyShader != material->GetPixelShader(false));
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

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"registry ownership", RegistryOwnership}, {"registration failure ownership", RegistryFailures},
        {"transform hierarchy and cache", TransformHierarchy}, {"unregister hierarchy and instances", UnregisterHierarchyAndInstances},
        {"frame light snapshot", LightSnapshot}, {"forward/deferred shader variants", ShaderVariants},
        {"shader includes, overrides and dependencies", ShaderCompilerIncludes}, {"shader invalidation and errors", ShaderInvalidation},
        {"material template variants and reflection", MaterialTemplateVariants},
        {"mesh primitives: normals, uvs, winding", MeshPrimitivesGeometry}, {"mesh component offscreen rendering", MeshComponentRendering},
        {"material compilation, parameters, errors", MaterialCompilationAndParameters}, {"material offscreen rendering", MaterialOffscreenRendering},
        {".mat format: load, validate, save", MaterialAssetFormat}, {"game material cache and reload", GameMaterialCache},
        {"materials on components and passes", MaterialComponentIntegration},
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
