#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Sound/AudioEngine.h>

ENGINE_GAME("ProjectGame")
class ProjectGame : public Game
{
public:
    void AfterInitialize() override;
    void PreUpdate(float deltaTime) override;
    void TickComponents(float deltaTime) override;

private:
    int saveTestMode_ = 0;
    int saveTestStep_ = 0;
    float saveTestX_ = 0.0f;
    bool frameDumpRequested_ = false;
    bool instanceTestDone_ = false;
    std::string audioCase_;
    audio::AudioEngine::SoundId audioSound_{};
    audio::AudioEngine::StreamHandle audioStream_{};
    float audioElapsed_ = 0.0f;
    bool audioStopped_ = false;
    bool audioPlayPending_ = false;
    int audioWarmFrames_ = 0;
    float audioEmitterDistance_ = 0.0f;
};
