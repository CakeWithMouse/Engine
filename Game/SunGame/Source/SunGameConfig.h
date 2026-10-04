#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>


class SunGameConfig
{
public:
    RenderType RenderingType = Deffered;

    //Light
    float SunDistanceMult = 1.f;
    glm::vec3 SunDirectional = glm::vec3(-0.35f, -0.85f, -0.2f) * SunDistanceMult;
    glm::vec3 SunLightColor = glm::vec3(0.95f, 0.93f, 0.90f);
    float SunItensity = 0.40f;
    float SkyboxPower = 0.8f;
    
    //Shadows
    float ShadowDist = 14000.f;
    bool ShadowCasting = true;
};
