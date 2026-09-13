#pragma once

#include "../../../../Source/Public/Components/GameComponents.h"

class PointLightComponent : public GameComponent
{
public:
    PointLightComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color, float intensity, float radius)
        : GameComponent(pos, rot, scale, color),
          LightColor(color.r, color.g, color.b),
          Intensity(intensity),
          Radius(radius)
    {
    }
    
    explicit PointLightComponent(glm::vec3 pos, glm::vec3 color, float intensity = 1.0f, float radius = 50.0f)
        : GameComponent(pos, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec4(color, 1.0f)),
          LightColor(color),
          Intensity(intensity),
          Radius(radius)
    {
    }
    
    void CreateBuffers(Microsoft::WRL::ComPtr<ID3D11Device> Device) override {}
    void Render(ID3D11DeviceContext* context) override {}
    
    glm::vec3 GetLightColor() const { return LightColor; }
    float GetIntensity() const { return Intensity; }
    float GetRadius() const { return Radius; }
    bool IsEnabled() const { return bEnabled; }
    
    void SetLightColor(const glm::vec3& color) { LightColor = color; }
    void SetIntensity(float intensity) { Intensity = intensity; }
    void SetRadius(float radius) { Radius = radius; }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    
private:
    glm::vec3 LightColor{1.0f, 1.0f, 1.0f};
    float Intensity{1.0f};
    float Radius{50.0f};
    bool bEnabled{true};
};
