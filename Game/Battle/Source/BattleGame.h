#pragma once

#include "UnitMesh.h"

#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <array>
#include <string>
#include <vector>

/**
 * Measurement scene for mass vehicle rendering: a field of instanced units of several types seen
 * from a strategy camera. One component per model type, so the number of draw calls depends on the
 * number of types and detail levels, not on the number of vehicles.
 */
class BattleGame : public Game
{
public:
    struct Settings
    {
        int UnitCount = 30000;
        float Spacing = 7.0f;
        // Distance from the camera to the centre of the field; 0 picks it from the field size.
        float CameraRadius = 0.0f;
        // Camera pitch above the horizon, degrees.
        float CameraPitch = 55.0f;
        bool Shadows = true;
        bool LodDebugColors = false;
        bool Forward = false;
        bool Moving = true;
        bool Impostors = true;
        // Tessellation multiplier of the stand-in models: 1 is a light placeholder, 4 is close
        // to a real vehicle asset.
        int Detail = 1;
        // On-screen height below which a unit becomes a sprite.
        float SpritePixels = 12.0f;
        // How many vehicle types take part (1 keeps the single-model scene for comparisons).
        int VehicleTypes = static_cast<int>(UnitMesh::VehicleType::Count);
        // Static map objects; their shadow is baked once when BakedShadows is on.
        int SceneryCount = 300;
        bool BakedShadows = true;
        // Sun elevation above the horizon, degrees. Low sun makes shadows long and easy to check.
        float SunElevation = 56.0f;
        // Diagnostics: writes the shadow maps to BMP files after a few frames.
        bool DumpShadows = false;
    };

    void BuildScene(const Settings& settings);
    /** Quits after this many frames; used for unattended measurement runs. */
    void SetFrameLimit(uint64_t frames) { FrameLimit = frames; }
    /** One line per run: units, frame time, GPU time, draw calls, instances. */
    std::string BuildReport() const;

protected:
    void PreUpdate(float deltaTime) override;
    std::array<float, 4> GetClearColor() const override { return {0.42f, 0.52f, 0.62f, 1.0f}; }

private:
    /** One model type: its component and the speed of every unit of that type. */
    struct UnitBatch
    {
        InstancedMeshComponent* Component = nullptr;
        UnitMesh::VehicleType Type = UnitMesh::VehicleType::Tank;
        std::vector<float> Speeds;
    };

    InstancedMeshComponent* CreateUnitBatch(UnitMesh::VehicleType type, const std::string& shaderPath);
    void UpdateUnits(float deltaTime);
    void UpdateHotkeys();
    uint32_t CountForLod(int level) const;

    std::vector<UnitBatch> UnitBatches;
    InstancedMeshComponent* Scenery = nullptr;
    Settings Config;
    float FieldHalfSize = 0.0f;
    bool bLodDebugKeyWasDown = false;

    // Measurements of this run (skipping the first frames, which include resource creation).
    uint64_t FrameLimit = 0;
    uint64_t MeasuredFrames = 0;
    double FrameMsSum = 0.0;
    double GpuMsSum = 0.0;
    uint64_t DrawCallSum = 0;
    uint64_t InstancesSum = 0;
    double LastTimestampMs = 0.0;
};
