#include "BattleBench.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <d3d11.h>
#include <dxgi.h>
#include <intrin.h>
#include <iostream>
#include <wrl/client.h>

void BattleBenchSession::Initialize(uint64_t targetFrames, uint64_t warmupFrames, int latency)
{
    (void)latency;
    bBenchActive = true;
    TargetFrames = targetFrames;
    WarmupFrames = warmupFrames;
    GpuSamplesExpected = targetFrames;

    SampleCapacity = static_cast<size_t>(targetFrames + 128);
    CpuFrameSamples.assign(SampleCapacity, 0.0);
    GpuFrameSamples.assign(SampleCapacity, 0.0);
    CpuSampleCount = 0;
    GpuSampleCount = 0;
    bCpuOverflowWarned = false;
    bGpuOverflowWarned = false;
    LastSeenGpuSamples = 0;
}

void BattleBenchSession::OnWarmupFrame(uint64_t readyGpuSamples)
{
    LastSeenGpuSamples = readyGpuSamples;
}

void BattleBenchSession::RecordCpuSample(double frameDurationMs)
{
    if (CpuSampleCount < TargetFrames)
    {
        if (CpuSampleCount < SampleCapacity)
        {
            CpuFrameSamples[CpuSampleCount++] = frameDurationMs;
        }
        else if (!bCpuOverflowWarned)
        {
            bCpuOverflowWarned = true;
            std::cerr << "Warning: CPU sample buffer overflow, stopping sample recording." << std::endl;
        }
    }
}

void BattleBenchSession::RecordGpuSamples(uint64_t readyGpuSamples, float lastGpuFrameMs)
{
    if (readyGpuSamples != LastSeenGpuSamples)
    {
        const uint64_t newSamples = readyGpuSamples - LastSeenGpuSamples;
        LastSeenGpuSamples = readyGpuSamples;
        for (uint64_t s = 0; s < newSamples; ++s)
        {
            if (GpuSampleCount < TargetFrames)
            {
                if (GpuSampleCount < SampleCapacity)
                {
                    GpuFrameSamples[GpuSampleCount++] = static_cast<double>(lastGpuFrameMs);
                }
                else if (!bGpuOverflowWarned)
                {
                    bGpuOverflowWarned = true;
                    std::cerr << "Warning: GPU sample buffer overflow, stopping sample recording." << std::endl;
                }
            }
        }
    }
}

double BattleBenchSession::Percentile(const std::vector<double>& sorted, double p)
{
    if (sorted.empty()) return 0.0;
    if (sorted.size() == 1) return sorted.front();
    const double rank = p * static_cast<double>(sorted.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(rank));
    const size_t upper = static_cast<size_t>(std::ceil(rank));
    const double weight = rank - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

std::string BattleBenchSession::GetGpuName(ID3D11Device* device)
{
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

std::string BattleBenchSession::GetCpuName()
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

std::string BattleBenchSession::GetGitCommit()
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

    return "unknown";
}

void BattleBenchSession::PrintHeader(int width, int height, bool vsync, int units, int latency, ID3D11Device* device) const
{
#ifdef _DEBUG
    const char* config = "Debug";
#else
    const char* config = "Release";
#endif

    std::cout << "=== Battle Benchmark Header ===\n"
              << "Git-Commit: " << GetGitCommit() << "\n"
              << "Configuration: " << config << "\n"
              << "CPU: " << GetCpuName() << "\n"
              << "GPU: " << GetGpuName(device) << "\n"
              << "Resolution: " << width << "x" << height << "\n"
              << "VSync: " << (vsync ? "enabled (INVALID)" : "disabled") << "\n"
              << "Units: " << units << "\n"
              << "Warmup: " << WarmupFrames << " frames\n"
              << "Bench-Frames: " << TargetFrames << " frames\n"
              << "Note: GPU timer queries lag behind CPU by GpuTimerLatency=" << latency << " frames.\n"
              << "===============================" << std::endl;
}

std::string BattleBenchSession::BuildReport(int units, uint64_t warmupFrames) const
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

    // For p99 tail: number of frames falling into the top 1% percentile
    const uint64_t p99Frames = static_cast<uint64_t>(std::ceil(static_cast<double>(CpuSampleCount) * 0.01));

#ifdef _DEBUG
    const char* config = "Debug";
#else
    const char* config = "Release";
#endif

    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer),
                  "battle-bench-result git=%s config=%s units=%d frames=%llu warmup=%llu cpu_median=%.2f cpu_p95=%.2f cpu_p99=%.2f p99_frames=%llu gpu_median=%.2f gpu_p95=%.2f gpu_p99=%.2f gpu_ready=%zu gpu_expected=%llu gpu_status=%s",
                  GetGitCommit().c_str(),
                  config,
                  units,
                  static_cast<unsigned long long>(CpuSampleCount),
                  static_cast<unsigned long long>(warmupFrames),
                  cpuMedian, cpuP95, cpuP99,
                  static_cast<unsigned long long>(p99Frames),
                  gpuMedian, gpuP95, gpuP99,
                  GpuSampleCount,
                  static_cast<unsigned long long>(GpuSamplesExpected),
                  gpuStatus);
    return buffer;
}
