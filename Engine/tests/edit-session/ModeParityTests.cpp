#define NOMINMAX
#include <Engine/Project/StandardTypes.h>
#include <Engine/Sound/AudioEngine.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void RequireParity(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
Game* factoryGame = nullptr;
GameComponent* latestMesh = nullptr;
bool failParityTick = false;
class ParityMesh final : public MeshComponent {
public:
    ParityMesh() { SetMesh(MeshPrimitives::Cube(2)); SetColor({1,1,1,1}); latestMesh = this; }
    void Tick(float) override { if (failParityTick) throw std::runtime_error("test tick failure"); }
};
ComponentTypeCatalog ProjectComponentTypes() {
    static const ComponentType types[] = {
        {"Parity.Mesh", "Mesh", EngineComponentDetail::Create<ParityMesh>, {}},
        {"Light.Directional", "Sun", EngineComponentDetail::Create<DirectionalLightComponent>, {}},
        {"Light.Point", "Lamp", []() -> std::unique_ptr<GameComponent> {
            return std::make_unique<PointLightComponent>(glm::vec3(0), glm::vec3(1), 2, 10);
        }, {}}};
    return {types, 3};
}
std::unique_ptr<Game> ParityGameFactory() {
    auto game = std::make_unique<Game>(); // Deliberately preserve the custom-factory default.
    game->GetSettings().SetVSync(false);
    factoryGame = game.get();
    return game;
}
}
#define SCENE_MODULE_CREATE_GAME ParityGameFactory
#include <Engine/Project/SceneModule.inl>

namespace audio {
// Inspect real XAudio voice progress, without adding diagnostics to the public API.
struct AudioTestAccess {
    static PooledVoice* Voice(AudioEngine& engine) {
        RequireParity(!engine.voicePool_.activeVoices_.empty(), "audio fixture has an active voice");
        return engine.voicePool_.activeVoices_.front();
    }
    static bool Empty(AudioEngine& engine) { return engine.voicePool_.activeVoices_.empty(); }
    static UINT64 Samples(PooledVoice* voice) {
        XAUDIO2_VOICE_STATE state{}; voice->voice->GetState(&state); return state.SamplesPlayed;
    }
};
}

namespace {
void WriteSilentWave(const std::filesystem::path& path) {
    std::vector<unsigned char> bytes(44 + 16000, 0); // One second, mono PCM16 at 8 kHz.
    std::memcpy(bytes.data(), "RIFF", 4); std::memcpy(bytes.data() + 8, "WAVEfmt ", 8);
    std::memcpy(bytes.data() + 36, "data", 4);
    auto number = [&](size_t offset, uint32_t value, int count) {
        for (int i = 0; i < count; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (i * 8));
    };
    number(4, static_cast<uint32_t>(bytes.size() - 8), 4); number(16, 16, 4);
    number(20, 1, 2); number(22, 1, 2); number(24, 8000, 4); number(28, 16000, 4);
    number(32, 2, 2); number(34, 16, 2); number(40, 16000, 4);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    RequireParity(bool(file), "write silent audio fixture");
}
std::vector<char> ReadCapture(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    RequireParity(bool(input), "frame capture exists");
    std::vector<char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    RequireParity(bytes.size() > 54, "frame capture contains pixels");
    return bytes;
}
void CheckAudioTransitions(SceneEditSession& session, Game& editGame,
                           const std::filesystem::path& wave) {
    using Access = audio::AudioTestAccess;
    auto* editAudio = editGame.GetAudioEngine();
    RequireParity(editAudio != nullptr, "audio device required for transition regression");
    const auto id = editAudio->LoadSound(wave.wstring());
    audio::AudioEngine::PlayOptions options; options.loop = true; options.volume = 0;
    RequireParity(editAudio->Play2D(id, options), "start silent edit preview fixture");
    const auto api = session.GetApi();
    RequireParity(api.control(api.context, EnginePlay) == EngineSceneOk, "audio Play");
    RequireParity(Access::Empty(*editAudio), "Play stops the edit scene's preview voice");
    auto* audio = factoryGame->GetAudioEngine();
    RequireParity(audio != nullptr, "Play audio device");
    const auto sound = audio->LoadSound(wave.wstring());
    RequireParity(audio->Play2D(sound, options), "start silent game voice");
    auto* voice = Access::Voice(*audio);
    Sleep(80);
    RequireParity(Access::Samples(voice) > 0, "game voice advances");
    RequireParity(api.control(api.context, EnginePause) == EngineSceneOk, "audio Pause");
    const auto paused = Access::Samples(voice);
    Sleep(80);
    RequireParity(api.frame(api.context, 0.01) == EngineSceneOk && Access::Samples(voice) == paused,
                  "Pause freezes audio through rendered frames");
    RequireParity(api.control(api.context, EngineResume) == EngineSceneOk, "audio Resume");
    Sleep(80);
    RequireParity(Access::Samples(voice) > paused, "Resume continues audio position");
    failParityTick = true;
    const auto status = api.frame(api.context, 0.01);
    failParityTick = false;
    RequireParity(status == EngineSceneSimulationFailed, "test simulation failure");
    const auto failed = Access::Samples(voice);
    Sleep(80);
    RequireParity(Access::Samples(voice) == failed, "simulation failure also pauses audio");
    RequireParity(api.control(api.context, EngineStop) == EngineSceneOk, "Stop paused audio");
    RequireParity(api.control(api.context, EnginePlay) == EngineSceneOk, "restart Play after audio Stop");
    audio = factoryGame->GetAudioEngine();
    RequireParity(audio && audio->Play2D(audio->LoadSound(wave.wstring()), options), "restarted Play audio");
    voice = Access::Voice(*audio); Sleep(80);
    RequireParity(Access::Samples(voice) > 0, "new Play is not left paused");
    RequireParity(api.control(api.context, EngineStop) == EngineSceneOk, "stop restarted Play");
}
}

void ExerciseModeParity(const std::filesystem::path& content) {
    const auto output = content.parent_path() / "build/mode-parity";
    std::filesystem::create_directories(output);
    const auto sceneFile = output / "parity.scene.json";
    const auto wave = output / "silence.wav";
    WriteSilentWave(wave);
    HWND window = CreateWindowExW(0, L"STATIC", L"Mode parity", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    RequireParity(window != nullptr, "parity HWND");
    struct Window { HWND value; ~Window() { DestroyWindow(value); } } cleanup{window};
    std::vector<char> darkPixels;
    for (int lighting = 0; lighting < 4; ++lighting) {
        EnginePaths::SetContentRoot(content);
        SceneEditSession editor(ProjectComponentTypes(), ParityGameFactory);
        const auto edit = editor.GetApi();
        uint64_t meshId = 0, sunId = 0;
        RequireParity(edit.scene.createObject(edit.scene.context, "Parity.Mesh", "Cube", nullptr, &meshId) == EngineSceneOk, "parity mesh");
        auto* editMesh = latestMesh;
        if (lighting == 1 || lighting == 2) {
            RequireParity(edit.scene.createObject(edit.scene.context, "Light.Directional", "Sun", nullptr, &sunId) == EngineSceneOk, "parity sun");
        }
        if (lighting == 3) {
            const EngineSceneTransform transform{{0,1,3}, {0,0,0,1}, {1,1,1}};
            RequireParity(edit.scene.createObject(edit.scene.context, "Light.Point", "Lamp", &transform, &sunId) == EngineSceneOk, "parity point light");
        }
        RequireParity(edit.attachViewport(edit.context, window, 320, 240) == EngineSceneOk, "parity Edit viewport");
        auto overlay = editor.GetViewportOverlayApi();
        RequireParity(overlay.setAntialiasing(overlay.context, 0) == EngineSceneOk, "parity AA");
        RequireParity(edit.frame(edit.context, 0) == EngineSceneOk, "parity Edit frame");
        auto* editGame = editMesh->GetGame();
        if (lighting == 2) {
            auto* sun = static_cast<DirectionalLightComponent*>(editGame->GetScene().FindComponent(sunId));
            auto data = sun->SaveProjectData();
            const auto enabled = data.find("\"enabled\":true");
            RequireParity(enabled != std::string::npos, "sun enabled fixture");
            data.replace(enabled, 14, "\"enabled\":false"); sun->LoadProjectData(data);
        }
        char error[512]{};
        RequireParity(edit.saveScene(edit.context, sceneFile.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "save parity scene");
        for (const auto renderer : {Forward, Deffered}) {
            const std::string label = std::to_string(lighting) + (renderer == Forward ? "-forward" : "-deferred");
            editGame->GetSettings().SetRenderingType(renderer);
            const auto editFile = output / (label + "-edit.bmp");
            editGame->RequestFrameDump(editFile.string());
            RequireParity(edit.frame(edit.context, 0) == EngineSceneOk, "capture Edit");
            const auto reference = ReadCapture(editFile);
            if (!lighting && renderer == Forward) darkPixels = reference;
            if (lighting == 1 || lighting == 3) RequireParity(reference != darkPixels, "light visibly lights the fixture");
            if (lighting == 2 && renderer == Forward) RequireParity(reference == darkPixels, "disabled sun returns to dark scene");
            RequireParity(edit.control(edit.context, EnginePlay) == EngineSceneOk, "parity Play");
            factoryGame->GetSettings().SetRenderingType(renderer);
            const auto playFile = output / (label + "-play.bmp");
            factoryGame->RequestFrameDump(playFile.string());
            RequireParity(edit.frame(edit.context, 0) == EngineSceneOk, "capture Play");
            RequireParity(ReadCapture(playFile) == reference, "Edit and Play pixels match");
            RequireParity(edit.control(edit.context, EngineStop) == EngineSceneOk, "parity Stop");

            EngineModuleHost host{ENGINE_MODULE_VERSION, nullptr, [](void*, const char*) {}, [](void*) {}, "parity", "", ""};
            auto* runtime = static_cast<Session*>(Create(&host));
            RequireParity(runtime != nullptr, "custom factory runtime session");
            struct Runtime { Session* value; std::filesystem::path content;
                ~Runtime() { Destroy(value); EnginePaths::SetContentRoot(content); } } destroy{runtime, content};
            EngineGameSceneApi loader{};
            RequireParity(EngineGetGameSceneApi(ENGINE_GAME_SCENE_API_VERSION, sizeof(loader), runtime, &loader) != 0, "runtime loader");
            RequireParity(loader.loadScene(loader.context, sceneFile.u8string().c_str(), error, sizeof(error)) == EngineSceneOk, "runtime loads saved scene");
            auto& game = *runtime->game;
            game.GetSettings().SetRenderingType(renderer);
            RequireParity(game.InitializeRuntimeWindow(false), "hidden standalone viewport");
            game.GetPlayer()->SetPosition({0,0,5});
            game.SetSimulationTimeScale(1);
            game.OnWindowResized(320, 240, false);
            const auto runtimeFile = output / (label + "-runtime.bmp");
            game.RequestFrameDump(runtimeFile.string());
            RequireParity(Update(runtime, 0) == 1, "capture standalone Game");
            RequireParity(game.GetFrameConstants().LightMeta.y == 0 && game.GetFrameConstants().LightMeta.w == 1,
                          "custom factory scene load preserves scene lighting");
            RequireParity(ReadCapture(runtimeFile) == reference, "Edit, Play and standalone pixels match");
            if (renderer == Forward && lighting < 2) {
                // Reproduce the old custom-factory mode on the very same GPU scene.
                game.SetSceneLightingEnabled(false);
                const auto legacyFile = output / (label + "-before-fix.bmp");
                game.RequestFrameDump(legacyFile.string());
                RequireParity(Update(runtime, 0) == 1 && ReadCapture(legacyFile) != reference,
                              "old lighting mode visibly differs from the editor");
            }
            EnginePaths::SetContentRoot(content);
        }
        EnginePaths::SetContentRoot(content);
        if (!lighting) CheckAudioTransitions(editor, *editGame, wave);
    }
    EnginePaths::SetContentRoot(content);
    std::cout << "Mode parity: 24 GPU captures match; old lighting mismatch reproduced; audio Play/Pause/Resume/failure/Stop passed\n";
}
