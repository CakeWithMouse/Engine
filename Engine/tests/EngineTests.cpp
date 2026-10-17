#include <Engine/Resources/Paths.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/ParticleSystemComponent.h>
#include <Engine/Render/MeshSimplifier.h>
#include <Engine/Render/MeshValidation.h>
#include <Engine/Render/ShaderCompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
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
    }
}

void QuitMessage()
{
    TestGame game;
    PostQuitMessage(17);
    CHECK(!game.PumpQuit());
    CHECK(game.GetExitCode() == 17);
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

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"registry ownership", RegistryOwnership}, {"registration failure ownership", RegistryFailures},
        {"transform hierarchy and cache", TransformHierarchy}, {"unregister hierarchy and instances", UnregisterHierarchyAndInstances},
        {"frame light snapshot", LightSnapshot}, {"forward/deferred shader variants", ShaderVariants},
        {"particle GPU indexing", ParticleShaderIndexing}, {"particle component sorting modes", ParticleComponentModes},
        {"WM_QUIT exit code", QuitMessage},
        {"mesh simplification", MeshSimplification}, {"mesh validation rules", MeshValidationRules}
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
