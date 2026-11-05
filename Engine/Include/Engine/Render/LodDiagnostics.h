#pragma once
#include <cstdint>
#include <string>
#include <vector>

/** CPU color-pass selection, not shadow submissions or GPU timing. */
struct LodDiagnostics {
    uint64_t componentId=0, frameIndex=0;
    std::string name;
    bool prepared=false, screenSize=false;
    uint32_t total=0, drawn=0, hidden=0, distanceCulled=0, frustumCulled=0;
    bool hasDistance=false;
    float minDistance=0, maxDistance=0, drawDistance=0, fadeFraction=0;
    int impostorLevel=-1;
    std::vector<float> thresholds;
    // A cross-fading instance occupies two buckets but counts once in drawn.
    std::vector<uint32_t> perLod;
};
