#pragma once

#include "BattleBench.h"
#include "UnitMesh.h"

#include <Engine/Components/SpecificComponents/UIComponent.h>

#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Ai { class Context; class Group; }

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
        float Spacing = 8.0f;
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
        // Past this distance from the camera a unit stops casting a shadow.
        float ShadowDistance = 900.0f;
        // Diagnostics: writes the shadow maps to BMP files after a few frames.
        bool DumpShadows = false;
        // Diagnostics: writes the baked sprite atlas to BMP files after a few frames.
        bool DumpImpostors = false;
        // Diagnostics: writes the finished frame to this BMP file, then keeps running.
        std::string DumpFrame;
        // The frame that DumpFrame saves.
        uint64_t DumpFrameAt = 60;
        // Deterministic camera motion, one frame and matrix sample per step (60..180).
        std::string CameraSequence;
        std::string CameraMotion = "orbit";
        // Ignores the mouse and the keyboard, so two runs of the same scene frame it identically.
        // Screenshots taken for comparison are only readable with this on.
        bool LockCamera = false;
        // Supply only LOD0 and let the engine build the coarser levels from it.
        bool AutoLod = false;
        // Check LOD0 against the model contract and print a report.
        bool ValidateAssets = false;
        // On-screen panels in the corner of the scene; F7 toggles them at runtime.
        bool Hud = false; // Demo statistics are opt-in, like project HUDs.
        // Warm point light over the centre of the field, and whether it casts shadows.
        bool PointLight = true;
        bool PointShadows = true;
        // Units and scenery use the material assets of GameContent/Materials; false draws them with
        // the old InstancedMesh shader (same look without mud), for comparisons.
        bool Materials = true;
        std::filesystem::path ContentRoot;
        // Animated crowd using GPU baked VAT (CesiumMan).
        bool Animated = false;
        // Regiments driven by the engine AI: advance, engage, fire and withdraw.
        bool Ai = false;
        // The AI debugger is on from the first frame (as after F9).
        bool AiDebug = false;
    };

    void BuildScene(const Settings& settings);
    /** Quits after this many frames; used for unattended measurement runs. */
    void SetFrameLimit(uint64_t frames) { FrameLimit = frames; }
    /** One line per run: units, frame time, GPU time, draw calls, instances. */
    std::string BuildReport() const;

    /** Enables robust benchmark mode with pre-allocated sample buffers. */
    void EnableBench(uint64_t benchFrames, uint64_t warmupFrames = 150);
    bool IsBenchMode() const { return Bench.IsBenchActive(); }
    bool IsVSyncActive() const { return GetSettings().IsVSyncEnabled(); }

    void PrintBenchHeader() const;
    std::string BuildBenchReport() const;

protected:
    void PreUpdate(float deltaTime) override;
    void TickComponents(float deltaTime) override;
    std::array<float, 4> GetClearColor() const override { return {0.42f, 0.52f, 0.62f, 1.0f}; }

private:
    /** One model type: its component and the speed of every unit of that type. */
    struct UnitBatch
    {
        InstancedMeshComponent* Component = nullptr;
        UnitMesh::VehicleType Type = UnitMesh::VehicleType::Tank;
        std::vector<float> Speeds;
    };

    /** material: null draws the batch with the shader at shaderPath. */
    InstancedMeshComponent* CreateUnitBatch(UnitMesh::VehicleType type, const std::string& shaderPath,
                                            const std::shared_ptr<Material>& material);
    void UpdateUnits(float deltaTime);
    void UpdateHotkeys();
    uint32_t CountForLod(int level) const;
    /** Debug colours per detail level, from F6 and from the button in the HUD. */
    void SetLodDebugColors(bool enabled);
    /** Reads the panels from GameContent/UI/Hud.ui; UpdateHud only changes their text. */
    void BuildHud();
    /**
     * Looks the elements the scene writes to up in the markup. Called again after a hot reload,
     * because rebuilding the tree invalidates every pointer into it.
     */
    void BindHud();
    void UpdateHud();
    /** Navigation from the field and the buildings, regiments from the vehicles; once, after Play starts. */
    void SetupAi();
    void UpdateAiHud();
    void UpdateCombatEffects(float deltaTime);
    void FireSalvo(Ai::Context& ctx, Ai::Group& regiment, Ai::Group& enemy);
    /** " ai_states=Maneuver/Hold:12,..." for the result line: how many regiments are in each state. */
    std::string DescribeAiStates() const;
    /** Shows or hides the AI debugger; the regiment nearest the centre of the field is selected. */
    void ShowAiDebug(bool visible);

    class FireAtEnemy;
    struct ShotEffect
    {
        glm::vec3 from;
        glm::vec3 to;
        uint32_t targetRegiment = 0;
        size_t targetMember = 0;
        float age = 0.0f;
        bool hit = false;
        bool destroyed = false;
    };

    UIComponent* Hud = nullptr;
    UILabel* HudFrameLabel = nullptr;
    UILabel* HudDrawLabel = nullptr;
    UILabel* HudUnitLabel = nullptr;
    UILabel* HudAiStats = nullptr;
    UILabel* HudAiState = nullptr;
    UILabel* HudAiAlive = nullptr;
    UILabel* HudAiCombat = nullptr;
    UILabel* HudLodLabels[4] = {};
    UIBar* HudFrameBar = nullptr;
    double HudFrameMs = 0.0;
    /** The markup revision the pointers above were taken from. */
    uint32_t HudRevision = 0;
    bool bHudKeyWasDown = false;

    std::vector<UnitBatch> UnitBatches;
    InstancedMeshComponent* Scenery = nullptr;
    Settings Config;
    float FieldHalfSize = 0.0f;
    bool bLodDebugKeyWasDown = false;

    // Measurements of this run (skipping the first frames, which include resource creation).
    uint64_t FrameLimit = 0;
    uint64_t UpdateCount = 0; // Local benchmark progress; independent of the renderer's frame index.
    float SequenceCameraRadius = 0.0f;
    uint64_t MeasuredFrames = 0;
    double FrameMsSum = 0.0;
    double GpuMsSum = 0.0;
    uint64_t DrawCallSum = 0;
    uint64_t InstancesSum = 0;
    double LastTimestampMs = 0.0;

    // Benchmark mode session
    BattleBenchSession Bench;
    uint64_t WarmupFrames = 10;

    int UnitVertexCount = 0;

    bool bAiReady = false;
    bool bAiDebugKeyWasDown = false;
    size_t Regiments = 0;
    double LastAiMs = 0.0;
    double AiMsSum = 0.0;
    std::map<uint32_t, std::vector<int>> HealthByRegiment;
    std::vector<ShotEffect> ShotEffects;
    std::array<int, 2> AliveByTeam{};
    int ShotsFired = 0;
    int Hits = 0;
    int Destroyed = 0;
};
