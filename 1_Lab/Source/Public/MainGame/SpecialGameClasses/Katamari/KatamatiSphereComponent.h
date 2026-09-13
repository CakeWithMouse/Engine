#pragma once
#include "../../../../../Source/Public/Components/GameComponents.h"
#include "../../../../includes/GLM-master/glm/ext/quaternion_trigonometric.hpp"
#include "../../../../includes/GLM-master/glm/gtc/quaternion.hpp"
#include <vector>
#include <cmath>

struct CollectibleObject
{
    GameComponent* component;
    glm::vec3 startPosition;
    glm::vec3 targetPosition;
    glm::vec3 hitDirection;
    float progress;
    bool isCollecting;
};

struct AttachedObject
{
    GameComponent* component;
    glm::vec3 relativePosition; 
    glm::vec3 relativeRotation;
    glm::vec3 attachDirection;
};

class KatamatiSphereComponent : public GameComponent
{
public:
    KatamatiSphereComponent(){};
    KatamatiSphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    KatamatiSphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    
    void InitSphere(float radius = 0.5f, int slices = 20, int stacks = 20);
    void Tick(float deltaTime) override;
    DirectX::XMFLOAT4X4 GetWorldMatrix() override;
    void SetSpeed(float speed){Speed = speed;};
    
    void CheckCollisions(std::vector<GameComponent*> ComponentsWithCollision);
    void CollectComponent(GameComponent* component, const glm::vec3& hitPoint);
    void UpdateCollectibles(float deltaTime);
    void UpdateAttachedObjects();
    
    void AcceptRotation(const glm::vec3& moveDirection, float moveDistance);
    
private:
    void GenerateSphere(float radius, int slices, int stacks);
    std::vector<DirectX::XMFLOAT4> vertices;
    std::vector<int> indices;
    float Speed = 1.f;
    float Radius = 1.f;
    
    float GrowthFactor = 1.05f;
    float CollectAnimationTime = 0.5f;
    
    float minSpeed = 0.2f;
    float maxSpeed = 20.0f;
    float zoomSpeed = 1.0f;
    float jumpImpulse = 50.0f;
    float gravityStrength = 10.0f;
    float verticalVelocity = 0.0f;
    int jumpsUsed = 0;
    int maxJumpCount = 2;
    bool bJumpKeyWasDown = false;
    
    float rotationAngle = 0.0f;
    glm::vec3 lastMoveDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    
    std::vector<CollectibleObject> collectingObjects;
    std::vector<AttachedObject> attachedObjects;
    glm::vec3 lastHitPoint;
    
    glm::quat ComponentQuaternion = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};
