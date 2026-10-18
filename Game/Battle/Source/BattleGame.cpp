#include "BattleGame.h"

#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Resources/Paths.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <glm/trigonometric.hpp>
#include <intrin.h>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

namespace
{
    double NowMs()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
    }

    double Percentile(const std::vector<double>& sorted, double p)
    {
        if (sorted.empty()) return 0.0;
        if (sorted.size() == 1) return sorted.front();
        const double rank = p * static_cast<double>(sorted.size() - 1);
        const size_t lower = static_cast<size_t>(std::floor(rank));
        const size_t upper = static_cast<size_t>(std::ceil(rank));
        const double weight = rank - static_cast<double>(lower);
        return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
    }

    template <class T>
    T* Register(Game& game, const std::string& name, std::unique_ptr<T> component,
                const std::string& pixelShader = {}, const std::string& vertexShader = {})
    {
        const RegisterResult result = game.RegisterComponent(name, component.get(), pixelShader, vertexShader);
        if (result != RegisterResult::Ok)
        {
            throw std::runtime_error("Cannot register " + name + ": " + ToString(result));
        }
        return component.release();
    }
}

InstancedMeshComponent* BattleGame::CreateUnitBatch(UnitMesh::VehicleType type, const std::string& shaderPath)
{
    auto units = std::make_unique<InstancedMeshComponent>();
    for (int level = 0; level < 3; ++level)
    {
        MeshLodData lod = UnitMesh::BuildVehicleLod(type, level, Config.Detail);
        std::cout << UnitMesh::VehicleName(type) << " LOD" << level << ": "
                  << UnitMesh::CountTriangles(lod) << " triangles" << std::endl;
        if (!units->SetLod(level, std::move(lod)))
        {
            throw std::runtime_error(std::string("Cannot build LOD for ") + UnitMesh::VehicleName(type));
        }
    }

    // Switching by on-screen size: independent of model size, window size and field of view.
    // Three mesh levels plus the generated sprite level, so three thresholds.
    units->SetLodScreenHeights({45.0f, 16.0f, Config.SpritePixels});
    units->SetLodFadeFraction(0.06f);
    units->SetDrawDistance(2500.0f);
    units->SetShadowDistance(900.0f);
    units->SetShadowLodBias(1);
    units->SetLodDebugColors(Config.LodDebugColors);
    if (Config.Impostors)
    {
        units->SetImpostorEnabled(true);
    }

    return Register(*this, std::string("Units_") + UnitMesh::VehicleName(type), std::move(units),
                    shaderPath, shaderPath);
}

void BattleGame::BuildScene(const Settings& settings)
{
    Config = settings;
    RenderingType = Config.Forward ? Forward : Deffered;

    // One sun with cascaded shadows: the light model of a strategy battlefield.
    const float sunElevation = glm::radians(std::max(5.0f, std::min(Config.SunElevation, 89.0f)));
    const glm::vec3 sunDirection(-std::cos(sunElevation) * 0.83f, -std::sin(sunElevation), -std::cos(sunElevation) * 0.55f);
    SetDirectionalLight(sunDirection, glm::vec3(1.0f, 0.97f, 0.90f), 1.15f);
    SetShadowSettings(Config.Shadows, 900.0f);
    SetDeferredDebugOverlay(false);

    const int unitsPerRow = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(Config.UnitCount)))));
    const float fieldSize = unitsPerRow * Config.Spacing;
    FieldHalfSize = fieldSize * 0.5f;
    const std::string unitShaderPath = EnginePaths::Shader("InstancedMesh.hlsl").generic_u8string();
    // The baked map covers the whole field, so scenery shadows never follow the camera.
    SetStaticShadowArea(glm::vec3(0.0f, 0.0f, 0.0f), FieldHalfSize * 1.2f + 50.0f);

    auto ground = std::make_unique<CubeComponent>(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.0f),
                                                  glm::vec3(fieldSize * 1.5f, 1.0f, fieldSize * 1.5f),
                                                  glm::vec4(0.33f, 0.37f, 0.27f, 1.0f));
    ground->InitCube();
    ground->SetStaticShadowCaster(true);
    Register(*this, "Ground", std::move(ground));

    std::mt19937 rng(1337u);

    // Map scenery: never moves, so its shadow is baked once instead of being redrawn per cascade.
    if (Config.SceneryCount > 0)
    {
        const int sceneryTypeCount = static_cast<int>(UnitMesh::SceneryType::Count);
        std::uniform_real_distribution<float> place(-FieldHalfSize, FieldHalfSize);
        std::uniform_real_distribution<float> turn(0.0f, 6.283f);

        for (int typeIndex = 0; typeIndex < sceneryTypeCount; ++typeIndex)
        {
            const UnitMesh::SceneryType type = static_cast<UnitMesh::SceneryType>(typeIndex);
            auto scenery = std::make_unique<InstancedMeshComponent>();
            for (int level = 0; level < 2; ++level)
            {
                scenery->SetLod(level, UnitMesh::BuildSceneryLod(type, level, Config.Detail));
            }
            scenery->SetLodScreenHeights({40.0f, 10.0f});
            scenery->SetLodFadeFraction(0.06f);
            scenery->SetStaticShadowCaster(Config.BakedShadows);

            const int countForType = Config.SceneryCount / sceneryTypeCount +
                                     (typeIndex < Config.SceneryCount % sceneryTypeCount ? 1 : 0);
            scenery->ReserveInstances(static_cast<size_t>(countForType));
            for (int index = 0; index < countForType; ++index)
            {
                MeshInstance building;
                building.Position = glm::vec3(place(rng), 0.0f, place(rng));
                building.Yaw = turn(rng);
                building.Scale = 1.0f;
                building.Color = glm::vec4(0.55f, 0.52f, 0.47f, 1.0f);
                scenery->AddInstance(building);
            }
            InstancedMeshComponent* registered =
                Register(*this, std::string("Scenery_") + UnitMesh::SceneryName(type), std::move(scenery),
                         unitShaderPath, unitShaderPath);
            if (Scenery == nullptr)
            {
                Scenery = registered;
            }
        }
    }

    // One component per vehicle type; units are spread between them.
    const int typeCount = std::max(1, std::min(Config.VehicleTypes, static_cast<int>(UnitMesh::VehicleType::Count)));
    UnitBatches.reserve(static_cast<size_t>(typeCount));
    for (int typeIndex = 0; typeIndex < typeCount; ++typeIndex)
    {
        UnitBatch batch;
        batch.Type = static_cast<UnitMesh::VehicleType>(typeIndex);
        batch.Component = CreateUnitBatch(batch.Type, unitShaderPath);
        UnitBatches.push_back(std::move(batch));
    }

    std::uniform_real_distribution<float> jitter(-0.25f, 0.25f);
    std::uniform_real_distribution<float> speed(4.0f, 9.0f);
    const glm::vec4 teamColors[2] = {glm::vec4(0.35f, 0.42f, 0.30f, 1.0f), glm::vec4(0.45f, 0.33f, 0.28f, 1.0f)};

    for (int index = 0; index < Config.UnitCount; ++index)
    {
        const int row = index / unitsPerRow;
        const int column = index % unitsPerRow;
        // Types alternate along the row, so every formation mixes vehicles.
        UnitBatch& batch = UnitBatches[static_cast<size_t>((row + column) % typeCount)];

        MeshInstance instance;
        instance.Position = glm::vec3(
            (column - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing,
            0.0f,
            (row - unitsPerRow * 0.5f + jitter(rng)) * Config.Spacing);
        // Two armies facing each other across the middle of the field.
        const bool bSecondArmy = instance.Position.z > 0.0f;
        instance.Yaw = bSecondArmy ? 3.14159f : 0.0f;
        instance.Scale = 1.0f;
        instance.Color = teamColors[bSecondArmy ? 1 : 0];
        batch.Component->AddInstance(instance);

        const float typeSpeed = speed(rng) * UnitMesh::VehicleSpeedScale(batch.Type);
        batch.Speeds.push_back(bSecondArmy ? -typeSpeed : typeSpeed);
    }

    Player* player = GetPlayer();
    player->SetCameraMode(CameraMode::Orbital);
    player->SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    player->SetMinMaximalOrbitRadius(40.0f, std::max(600.0f, fieldSize * 1.5f));
    player->SetOrbitRadius(Config.CameraRadius > 0.0f ? Config.CameraRadius : std::max(180.0f, fieldSize * 0.37f));
    player->SetRotate(0.8f, glm::radians(std::max(10.0f, std::min(Config.CameraPitch, 85.0f))));
    player->StopRotate(true, true);
    player->EnableWASD(true);
}

void BattleGame::EnableBench(uint64_t benchFrames, uint64_t warmupFrames)
{
    bBenchMode = true;
    BenchTargetFrames = benchFrames;
    WarmupFrames = warmupFrames;
    GpuSamplesExpected = benchFrames;

    SampleCapacity = static_cast<size_t>(benchFrames + 128);
    CpuFrameSamples.assign(SampleCapacity, 0.0);
    GpuFrameSamples.assign(SampleCapacity, 0.0);
    CpuSampleCount = 0;
    GpuSampleCount = 0;
    bCpuOverflowWarned = false;
    bGpuOverflowWarned = false;
    LastSeenGpuSamples = 0;
    bVsyncViolation = false;

    // Run warmup + bench + latency frames to retire in-flight GPU queries.
    SetFrameLimit(warmupFrames + benchFrames + static_cast<uint64_t>(GpuTimerLatency));
}

void BattleGame::PreUpdate(float deltaTime)
{
    Game::PreUpdate(deltaTime);
    UpdateHotkeys();
    if (Config.Moving)
    {
        UpdateUnits(deltaTime);
    }

    if (bBenchMode)
    {
        if (bVSync)
        {
            bVsyncViolation = true;
            std::cerr << "Error: VSync is enabled during benchmark, measurement is invalid!" << std::endl;
            PostQuitMessage(1);
            return;
        }

        const double now = NowMs();
        const uint64_t frameIdx = GetFrameIndex();

        if (frameIdx <= WarmupFrames)
        {
            LastSeenGpuSamples = GpuTimerSamplesReady;
        }
        else
        {
            // Record CPU frame duration without allocations
            if (LastTimestampMs > 0.0 && CpuSampleCount < BenchTargetFrames)
            {
                if (CpuSampleCount < SampleCapacity)
                {
                    CpuFrameSamples[CpuSampleCount++] = now - LastTimestampMs;
                }
                else if (!bCpuOverflowWarned)
                {
                    bCpuOverflowWarned = true;
                    std::cerr << "Warning: CPU sample buffer overflow, stopping sample recording." << std::endl;
                }
            }

            // Record GPU timer sample when a new query result has been retired
            if (GpuTimerSamplesReady != LastSeenGpuSamples)
            {
                const uint64_t newSamples = GpuTimerSamplesReady - LastSeenGpuSamples;
                LastSeenGpuSamples = GpuTimerSamplesReady;
                for (uint64_t s = 0; s < newSamples; ++s)
                {
                    if (GpuSampleCount < BenchTargetFrames)
                    {
                        if (GpuSampleCount < SampleCapacity)
                        {
                            GpuFrameSamples[GpuSampleCount++] = static_cast<double>(LastGpuFrameMs);
                        }
                        else if (!bGpuOverflowWarned)
                        {
                            bGpuOverflowWarned = true;
                            std::cerr << "Warning: GPU sample buffer overflow, stopping sample recording." << std::endl;
                        }
                    }
                }
            }

            ++MeasuredFrames;
            FrameMsSum += (LastTimestampMs > 0.0 ? (now - LastTimestampMs) : 0.0);
            GpuMsSum += LastGpuFrameMs;
            DrawCallSum += LastFrameStats.DrawCalls;
            InstancesSum += LastFrameStats.InstancesDrawn;
        }
        LastTimestampMs = now;
    }
    else
    {
        const double now = NowMs();
        if (LastTimestampMs > 0.0 && GetFrameIndex() > WarmupFrames)
        {
            ++MeasuredFrames;
            FrameMsSum += now - LastTimestampMs;
            GpuMsSum += LastGpuFrameMs;
            DrawCallSum += LastFrameStats.DrawCalls;
            InstancesSum += LastFrameStats.InstancesDrawn;
        }
        LastTimestampMs = now;
    }

    if (Config.DumpShadows && GetFrameIndex() == 20)
    {
        DumpShadowMaps("shadow");
    }

    if (FrameLimit > 0 && GetFrameIndex() >= FrameLimit)
    {
        PostQuitMessage(0);
    }
}

void BattleGame::UpdateUnits(float deltaTime)
{
    // Every unit moves, so the instance streams are rebuilt and uploaded every frame: the
    // worst case for CPU cost, which is what the measurement should show.
    for (UnitBatch& batch : UnitBatches)
    {
        const size_t count = batch.Component->GetMeshInstanceCount();
        for (size_t index = 0; index < count; ++index)
        {
            const MeshInstance& instance = batch.Component->GetMeshInstance(index);
            glm::vec3 position = instance.Position;
            position.z += batch.Speeds[index] * deltaTime;
            if (position.z > FieldHalfSize) position.z -= 2.0f * FieldHalfSize;
            if (position.z < -FieldHalfSize) position.z += 2.0f * FieldHalfSize;
            batch.Component->SetInstancePosition(index, position);
        }
    }
}

void BattleGame::UpdateHotkeys()
{
    InputDevice* input = GetInputDevice();
    if (input == nullptr)
    {
        return;
    }

    const bool bDown = input->IsKeyDown(Keys::F6);
    if (bDown && !bLodDebugKeyWasDown)
    {
        Config.LodDebugColors = !Config.LodDebugColors;
        for (UnitBatch& batch : UnitBatches)
        {
            batch.Component->SetLodDebugColors(Config.LodDebugColors);
        }
    }
    bLodDebugKeyWasDown = bDown;
}

uint32_t BattleGame::CountForLod(int level) const
{
    uint32_t total = 0;
    for (const UnitBatch& batch : UnitBatches)
    {
        total += batch.Component->GetDrawnInstancesForLod(level);
    }
    return total;
}

std::string BattleGame::GetGpuName() const
{
    ID3D11Device* device = GetDevice();
    if (!device) return "unknown";

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()))))
    {
        return "unknown";
    }
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
    {
        return "unknown";
    }
    DXGI_ADAPTER_DESC desc{};
    if (FAILED(adapter->GetDesc(&desc)))
    {
        return "unknown";
    }

    char buffer[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buffer, sizeof(buffer), nullptr, nullptr);
    return std::string(buffer);
}

std::string BattleGame::GetCpuName()
{
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, static_cast<int>(0x80000000));
    const unsigned int maxExtendedId = static_cast<unsigned int>(cpuInfo[0]);
    if (maxExtendedId < 0x80000004)
    {
        return "unknown";
    }

    char brand[49] = {};
    __cpuid(reinterpret_cast<int*>(brand), static_cast<int>(0x80000002));
    __cpuid(reinterpret_cast<int*>(brand + 16), static_cast<int>(0x80000003));
    __cpuid(reinterpret_cast<int*>(brand + 32), static_cast<int>(0x80000004));
    brand[48] = '\0';

    const char* start = brand;
    while (*start == ' ') ++start;
    return std::string(start);
}

std::string BattleGame::GetGitCommit()
{
    char* envCommit = nullptr;
    size_t envLen = 0;
    if (_dupenv_s(&envCommit, &envLen, "GIT_COMMIT") == 0 && envCommit)
    {
        std::string res(envCommit);
        free(envCommit);
        if (!res.empty()) return res;
    }
    if (_dupenv_s(&envCommit, &envLen, "ENGINE_GIT_COMMIT") == 0 && envCommit)
    {
        std::string res(envCommit);
        free(envCommit);
        if (!res.empty()) return res;
    }

    try
    {
        std::filesystem::path cur = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i)
        {
            const std::filesystem::path gitPath = cur / ".git";
            if (std::filesystem::exists(gitPath))
            {
                if (std::filesystem::is_directory(gitPath))
                {
                    const std::filesystem::path headFile = gitPath / "HEAD";
                    if (std::filesystem::exists(headFile))
                    {
                        std::ifstream is(headFile);
                        std::string line;
                        if (std::getline(is, line))
                        {
                            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                                line.pop_back();
                            if (line.rfind("ref: ", 0) == 0)
                            {
                                const std::filesystem::path refPath = gitPath / line.substr(5);
                                if (std::filesystem::exists(refPath))
                                {
                                    std::ifstream refIs(refPath);
                                    std::string sha;
                                    if (std::getline(refIs, sha))
                                    {
                                        while (!sha.empty() && (sha.back() == '\r' || sha.back() == '\n' || sha.back() == ' '))
                                            sha.pop_back();
                                        if (sha.size() >= 7) return sha.substr(0, 7);
                                    }
                                }
                            }
                            else if (line.size() >= 7)
                            {
                                return line.substr(0, 7);
                            }
                        }
                    }
                }
                else if (std::filesystem::is_regular_file(gitPath))
                {
                    std::ifstream is(gitPath);
                    std::string line;
                    if (std::getline(is, line) && line.rfind("gitdir: ", 0) == 0)
                    {
                        std::string dir = line.substr(8);
                        while (!dir.empty() && (dir.back() == '\r' || dir.back() == '\n' || dir.back() == ' '))
                            dir.pop_back();
                        std::filesystem::path worktreeGit(dir);
                        std::filesystem::path headFile = worktreeGit / "HEAD";
                        if (std::filesystem::exists(headFile))
                        {
                            std::ifstream headIs(headFile);
                            std::string hline;
                            if (std::getline(headIs, hline))
                            {
                                while (!hline.empty() && (hline.back() == '\r' || hline.back() == '\n' || hline.back() == ' '))
                                    hline.pop_back();
                                if (hline.rfind("ref: ", 0) == 0)
                                {
                                    std::filesystem::path refPath = worktreeGit / hline.substr(5);
                                    if (!std::filesystem::exists(refPath))
                                    {
                                        std::filesystem::path commondirFile = worktreeGit / "commondir";
                                        if (std::filesystem::exists(commondirFile))
                                        {
                                            std::ifstream cIs(commondirFile);
                                            std::string cdir;
                                            if (std::getline(cIs, cdir))
                                            {
                                                while (!cdir.empty() && (cdir.back() == '\r' || cdir.back() == '\n' || cdir.back() == ' '))
                                                    cdir.pop_back();
                                                refPath = worktreeGit / cdir / hline.substr(5);
                                            }
                                        }
                                    }
                                    if (std::filesystem::exists(refPath))
                                    {
                                        std::ifstream refIs(refPath);
                                        std::string sha;
                                        if (std::getline(refIs, sha))
                                        {
                                            while (!sha.empty() && (sha.back() == '\r' || sha.back() == '\n' || sha.back() == ' '))
                                                sha.pop_back();
                                            if (sha.size() >= 7) return sha.substr(0, 7);
                                        }
                                    }
                                }
                                else if (hline.size() >= 7)
                                {
                                    return hline.substr(0, 7);
                                }
                            }
                        }
                    }
                }
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) break;
            cur = cur.parent_path();
        }
    }
    catch (...) {}

    return "unknown";
}

void BattleGame::PrintBenchHeader() const
{
    const int width = DisplayPtr ? DisplayPtr->GetWidth() : 0;
    const int height = DisplayPtr ? DisplayPtr->GetHeight() : 0;
#ifdef _DEBUG
    const char* config = "Debug";
#else
    const char* config = "Release";
#endif

    std::cout << "=== Battle Benchmark Header ===\n"
              << "Git-Commit: " << GetGitCommit() << "\n"
              << "Configuration: " << config << "\n"
              << "CPU: " << GetCpuName() << "\n"
              << "GPU: " << GetGpuName() << "\n"
              << "Resolution: " << width << "x" << height << "\n"
              << "VSync: " << (bVSync ? "enabled (INVALID)" : "disabled") << "\n"
              << "Units: " << Config.UnitCount << "\n"
              << "Warmup: " << WarmupFrames << " frames\n"
              << "Bench-Frames: " << BenchTargetFrames << " frames\n"
              << "Note: GPU timer queries lag behind CPU by GpuTimerLatency=" << GpuTimerLatency << " frames.\n"
              << "===============================" << std::endl;
}

std::string BattleGame::BuildBenchReport() const
{
    std::vector<double> cpuSorted(CpuFrameSamples.begin(), CpuFrameSamples.begin() + CpuSampleCount);
    std::sort(cpuSorted.begin(), cpuSorted.end());

    std::vector<double> gpuSorted(GpuFrameSamples.begin(), GpuFrameSamples.begin() + GpuSampleCount);
    std::sort(gpuSorted.begin(), gpuSorted.end());

    const double cpuMedian = Percentile(cpuSorted, 0.50);
    const double cpuP95 = Percentile(cpuSorted, 0.95);
    const double cpuP99 = Percentile(cpuSorted, 0.99);

    const double gpuMedian = Percentile(gpuSorted, 0.50);
    const double gpuP95 = Percentile(gpuSorted, 0.95);
    const double gpuP99 = Percentile(gpuSorted, 0.99);

    const bool bGpuReliable = (GpuSamplesExpected > 0) && (GpuSampleCount * 100 >= GpuSamplesExpected * 90);
    const char* gpuStatus = bGpuReliable ? "reliable" : "unreliable";

#ifdef _DEBUG
    const char* config = "Debug";
#else
    const char* config = "Release";
#endif

    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer),
                  "battle-bench-result git=%s config=%s units=%d frames=%llu warmup=%llu cpu_median=%.2f cpu_p95=%.2f cpu_p99=%.2f gpu_median=%.2f gpu_p95=%.2f gpu_p99=%.2f gpu_ready=%zu gpu_expected=%llu gpu_status=%s",
                  GetGitCommit().c_str(),
                  config,
                  Config.UnitCount,
                  static_cast<unsigned long long>(CpuSampleCount),
                  static_cast<unsigned long long>(WarmupFrames),
                  cpuMedian, cpuP95, cpuP99,
                  gpuMedian, gpuP95, gpuP99,
                  GpuSampleCount,
                  static_cast<unsigned long long>(GpuSamplesExpected),
                  gpuStatus);
    return buffer;
}

std::string BattleGame::BuildReport() const
{
    const double frames = static_cast<double>(MeasuredFrames == 0 ? 1 : MeasuredFrames);
    const double frameMs = FrameMsSum / frames;

    uint32_t sprites = 0;
    uint32_t culled = 0;
    bool bHasImpostors = false;
    for (const UnitBatch& batch : UnitBatches)
    {
        sprites += batch.Component->GetDrawnImpostors();
        culled += batch.Component->GetCulledInstances();
        bHasImpostors = bHasImpostors || batch.Component->HasImpostor();
    }

    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "units=%d types=%zu scenery=%d mode=%s shadows=%s sprites=%s frames=%llu frame_ms=%.2f fps=%.1f gpu_ms=%.2f draws=%.1f instances=%.0f lod0=%u lod1=%u lod2=%u sprite=%u culled=%u",
                  Config.UnitCount,
                  UnitBatches.size(),
                  Config.SceneryCount,
                  Config.Forward ? "forward" : "deferred",
                  Config.Shadows ? "on" : "off",
                  bHasImpostors ? "on" : "off",
                  static_cast<unsigned long long>(MeasuredFrames),
                  frameMs,
                  frameMs > 0.0 ? 1000.0 / frameMs : 0.0,
                  GpuMsSum / frames,
                  static_cast<double>(DrawCallSum) / frames,
                  static_cast<double>(InstancesSum) / frames,
                  CountForLod(0), CountForLod(1), CountForLod(2), sprites, culled);
    return buffer;
}
