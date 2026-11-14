#pragma once
#include <Engine/MainGame/GameSubsystem.h>
#include <Engine/SaveGame/SaveArchive.h>
ENGINE_SUBSYSTEM()
class CalmScoreSubsystem final : public GameSubsystem
{
public:
    int score = 0;
    void SerializeSaveGame(SaveArchive& archive) override
    {
        GameSubsystem::SerializeSaveGame(archive);
        archive.Section("CalmCity.Score", [&](SaveArchive& state) {
            state.Version(1);
            state.Field("score", score);
        });
    }
};
