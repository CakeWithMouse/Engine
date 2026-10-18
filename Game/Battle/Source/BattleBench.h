#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ID3D11Device;

/**
 * Pre-allocated, bounded benchmark session for CPU and GPU measurements.
 * Allocates all sample arrays in Initialize(); does zero allocations per frame.
 */
class BattleBenchSession
{
public:
    void Initialize(uint64_t targetFrames, uint64_t warmupFrames, int latency);

    bool IsBenchActive() const { return bBenchActive; }
    uint64_t GetWarmupFrames() const { return WarmupFrames; }
    uint64_t GetTargetFrames() const { return TargetFrames; }
    uint64_t GetExpectedGpuSamples() const { return GpuSamplesExpected; }

    void OnWarmupFrame(uint64_t readyGpuSamples);
    void RecordCpuSample(double frameDurationMs);
    void RecordGpuSamples(uint64_t readyGpuSamples, float lastGpuFrameMs);

    void PrintHeader(int width, int height, bool vsync, int units, int latency, ID3D11Device* device) const;
    std::string BuildReport(int units, uint64_t warmupFrames) const;

    static std::string GetGpuName(ID3D11Device* device);
    static std::string GetCpuName();
    static std::string GetGitCommit();

private:
    static double Percentile(const std::vector<double>& sorted, double p);

    bool bBenchActive = false;
    uint64_t WarmupFrames = 150;
    uint64_t TargetFrames = 600;
    uint64_t GpuSamplesExpected = 600;

    size_t SampleCapacity = 0;
    size_t CpuSampleCount = 0;
    size_t GpuSampleCount = 0;
    std::vector<double> CpuFrameSamples;
    std::vector<double> GpuFrameSamples;

    bool bCpuOverflowWarned = false;
    bool bGpuOverflowWarned = false;
    uint64_t LastSeenGpuSamples = 0;
};
