#pragma once
#include "../../../../Source/Public/Components/GameComponents.h"
#include <vector>
#include <cmath>

class SphereComponent : public GameComponent
{
public:
    SphereComponent(){};
    SphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    SphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    
    void InitSphere(float radius = 0.5f, int slices = 20, int stacks = 20);
    void Tick(float deltaTime) override;
    void SetSpeed(float speed){Speed = speed;};
    SphereComponent* CreateSphereInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color);
    
    void SetOrbitSpeed(float speed) { orbitSpeed = speed; }
    void SetOrbitDistance(float distance) { orbitDistance = distance; }
    void SetOrbitCenter(const glm::vec3& center) { orbitCenter = center; }
protected:
    float orbitSpeed = 0.0f;
    float orbitDistance = 0.0f;
    float currentAngle = 0.0f;
    glm::vec3 orbitCenter = glm::vec3(0.0f);
private:
    void GenerateSphere(float radius, int slices, int stacks);
    std::vector<DirectX::XMFLOAT4> vertices;
    std::vector<int> indices;
    float Speed = 1.f;
};
