#pragma once
#include "OrderedSceneGame.h"
#include "Scenes.h"
#include <Engine/Components/GameComponents.h>

// Reuse the BasicExamples geometry, with a fixed camera and a visible preview animation.
// The legacy sample's default camera looks away from the origin and its rotation speed
// is no longer consumed by the current material, so the preview sets transforms explicitly.
class BasicExamplesPreview : public OrderedSceneGame
{
public:
    void AfterInitialize() override
    {
        SetSimulationTimeScale(1.0f);
        BuildTriangles(*this);
        GetPlayer()->SetPosition({0.0f, 0.0f, -2.5f});
        GetPlayer()->Rotate(DirectX::XM_PI, 0.0f);
        FindComponent("1")->SetPosition({-0.65f, 0.0f, 0.0f});
        FindComponent("1")->SetColor({1.0f, 0.45f, 0.08f, 1.0f});
        FindComponent("2")->SetPosition({0.65f, 0.0f, 0.0f});
        FindComponent("2")->SetScale({1.5f, 1.5f, 1.5f});
        FindComponent("2")->SetColor({0.08f, 0.8f, 1.0f, 1.0f});
    }
protected:
    void PreUpdate(float dt) override
    {
        angle += dt * 45.0f;
        FindComponent("1")->SetRotation({0.0f, 0.0f, angle});
        FindComponent("2")->SetRotation({0.0f, 0.0f, -angle * 1.5f});
    }
private:
    float angle = 0.0f;
};
