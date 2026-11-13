#pragma once
#include <Engine/Components/ComponentAnnotations.h>
#include <Engine/Components/GameComponents.h>

ENGINE_COMPONENT("Audio tutorial", "Tutorial.Audio")
class AudioTutorial : public GameComponent {
public:
    void BeginPlay() override;
};
