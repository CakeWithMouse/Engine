#include "AudioTutorial.h"
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Runtime/ModuleVfs.h>
#include <Engine/Sound/AudioEngine.h>
#include <filesystem>
#include <stdexcept>

void AudioTutorial::BeginPlay() {
    GameComponent::BeginPlay();
    auto* game = GetGame();
    auto* sound = game ? game->GetAudioEngine() : nullptr;
    if (!sound) return; // The scene also runs on a machine without an audio device.
    const auto file = EngineModuleVfs::Resolve("/Game/Audio/tutorial.wav");
    if (file.empty()) throw std::runtime_error("Import tutorial.wav into Content/Audio");
    const auto id = sound->LoadSound(std::filesystem::u8path(file).wstring());
    if (!id.IsValid()) throw std::runtime_error("Cannot decode tutorial.wav");
    audio::AudioEngine::PlayOptions options;
    options.volume = 0.25f;
    if (!sound->Play2D(id, options)) throw std::runtime_error("Cannot play tutorial.wav");
}
