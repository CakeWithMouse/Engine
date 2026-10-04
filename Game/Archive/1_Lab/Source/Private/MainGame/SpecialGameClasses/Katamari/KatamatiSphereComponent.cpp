#include "../../../../../Source/Public/MainGame/SpecialGameClasses/Katamari/KatamatiSphereComponent.h"
#include <iostream>
#include <algorithm>
#include "../../../../includes/GLM-master/glm/ext/quaternion_trigonometric.hpp"
#include "../../../../includes/GLM-master/glm/gtc/quaternion.hpp"
#include "../../../../Public/Components/SpecificComponents/FBXComponent.h"

namespace
{
    DirectX::XMMATRIX RemoveScaleFromMatrixLocal(DirectX::XMMATRIX matrix)
    {
        using namespace DirectX;

        XMVECTOR scale, rotation, translation;
        XMMatrixDecompose(&scale, &rotation, &translation, matrix);

        return XMMatrixAffineTransformation(
            XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f),
            XMVectorZero(),
            rotation,
            translation
        );
    }
}

KatamatiSphereComponent::KatamatiSphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) 
    : GameComponent(pos, rot, scale)
{
    ComponentQuaternion = glm::quat(glm::radians(rot));
}

KatamatiSphereComponent::KatamatiSphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) 
    : GameComponent(pos, rot, scale, color)
{
    ComponentQuaternion = glm::quat(glm::radians(rot));
}

DirectX::XMMATRIX KatamatiSphereComponent::GetLocalMatrix() const
{
    using namespace DirectX;

    const XMMATRIX scaleMatrix = XMMatrixScaling(
        ComponentScale.x,
        ComponentScale.y,
        ComponentScale.z
    );

    const XMVECTOR quaternion = XMVectorSet(
        ComponentQuaternion.x,
        ComponentQuaternion.y,
        ComponentQuaternion.z,
        ComponentQuaternion.w
    );
    const XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(quaternion);

    const XMMATRIX translationMatrix = XMMatrixTranslation(
        ComponentPosition.x,
        ComponentPosition.y,
        ComponentPosition.z
    );

    return scaleMatrix * rotationMatrix * translationMatrix;
}

void KatamatiSphereComponent::CheckCollisions(std::vector<GameComponent*> ComponentsWithCollision)
{
    for (const auto& component : ComponentsWithCollision)
    {
        if (component == this) continue;
        if (!component->HasCollison()) continue;
        
        bool alreadyAttached = false;
        for (const auto& attached : attachedObjects)
        {
            if (attached.component == component)
            {
                alreadyAttached = true;
                break;
            }
        }
        if (alreadyAttached) continue;
        
        glm::vec3 delta = component->GetCenter() - GetCenter();
        float distance = glm::length(delta);
        const float objectBoundingRadius = component->GetBoundingRadius();
        
        if (distance <= Radius + objectBoundingRadius)
        {
            using namespace DirectX;

            const XMFLOAT4X4 sphereWorldMatrixFloat = GetWorldMatrix();
            XMMATRIX sphereWorldMatrix = XMLoadFloat4x4(&sphereWorldMatrixFloat);
            sphereWorldMatrix = RemoveScaleFromMatrixLocal(sphereWorldMatrix);

            const XMFLOAT4X4 objectWorldMatrixFloat = component->GetWorldMatrix();
            const XMMATRIX objectWorldMatrix = XMLoadFloat4x4(&objectWorldMatrixFloat);

            const XMMATRIX localMatrix = objectWorldMatrix * XMMatrixInverse(nullptr, sphereWorldMatrix);

            XMVECTOR localScaleVector;
            XMVECTOR localRotationVector;
            XMVECTOR localTranslationVector;
            XMMatrixDecompose(&localScaleVector, &localRotationVector, &localTranslationVector, localMatrix);

            XMFLOAT3 localPositionFloat3;
            XMStoreFloat3(&localPositionFloat3, localTranslationVector);
            glm::vec3 localPosition(
                localPositionFloat3.x,
                localPositionFloat3.y,
                localPositionFloat3.z
            );

            const float distanceFromCenter = glm::length(localPosition);
            if (distanceFromCenter > 0.01f)
            {
                localPosition = glm::normalize(localPosition) * (Radius + objectBoundingRadius);
            }

            XMVECTOR objectScaleVector;
            XMVECTOR objectRotationVector;
            XMVECTOR objectTranslationVector;
            XMMatrixDecompose(&objectScaleVector, &objectRotationVector, &objectTranslationVector, objectWorldMatrix);

            XMFLOAT4 objectRotationFloat4;
            XMStoreFloat4(&objectRotationFloat4, objectRotationVector);
            const glm::quat worldObjectRotation = glm::normalize(glm::quat(
                objectRotationFloat4.w,
                objectRotationFloat4.x,
                objectRotationFloat4.y,
                objectRotationFloat4.z
            ));

            XMVECTOR sphereScaleVector;
            XMVECTOR sphereRotationVector;
            XMVECTOR sphereTranslationVector;
            XMMatrixDecompose(&sphereScaleVector, &sphereRotationVector, &sphereTranslationVector, sphereWorldMatrix);

            XMFLOAT4 sphereRotationFloat4;
            XMStoreFloat4(&sphereRotationFloat4, sphereRotationVector);
            const glm::quat worldSphereRotation = glm::normalize(glm::quat(
                sphereRotationFloat4.w,
                sphereRotationFloat4.x,
                sphereRotationFloat4.y,
                sphereRotationFloat4.z
            ));

            const glm::quat localObjectRotation = glm::normalize(glm::inverse(worldSphereRotation) * worldObjectRotation);

            
            component->SetParentWithoutScale(this);
            component->SetPosition(localPosition);
            component->SetRotationQuat(localObjectRotation);
            component->MarkTransformDirty();
            
            attachedObjects.push_back({
                component,
                localPosition,
                glm::degrees(glm::eulerAngles(localObjectRotation)),
                glm::normalize(delta)
            });
            
            const float previousRadius = Radius;
            Radius *= GrowthFactor;
            const float visualScaleFactor = Radius / std::max(previousRadius, 0.001f);
            ComponentScale *= visualScaleFactor;
        }
    }
}


void KatamatiSphereComponent::UpdateCollectibles(float deltaTime)
{
}

void KatamatiSphereComponent::UpdateAttachedObjects()
{
}

void KatamatiSphereComponent::AcceptRotation(const glm::vec3& moveDirection, float moveDistance)
{
    if (moveDistance <= 0.0001f || glm::dot(moveDirection, moveDirection) <= 0.0001f)
    {
        return;
    }

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 rotationAxis = glm::cross(worldUp, glm::normalize(moveDirection));

    if (glm::dot(rotationAxis, rotationAxis) <= 0.0001f)
    {
        return;
    }

    rotationAxis = glm::normalize(rotationAxis);
    const float rotationAngleRadians = moveDistance / std::max(Radius, 0.001f);
    const glm::quat deltaRotation = glm::angleAxis(rotationAngleRadians, rotationAxis);

    ComponentQuaternion = glm::normalize(deltaRotation * ComponentQuaternion);
    ComponentRotation = glm::degrees(glm::eulerAngles(ComponentQuaternion));
}

void KatamatiSphereComponent::GenerateSphere(float radius, int slices, int stacks)
{
    vertices.clear();
    indices.clear();
    Radius = radius;
    
    for (int i = 0; i <= stacks; ++i)
    {
        float phi = glm::pi<float>() * float(i) / float(stacks);
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);
        
        float v = 1.0f - (float)i / stacks;
        
        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * glm::pi<float>() * float(j) / float(slices);
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            
            float x = radius * sinPhi * cosTheta;
            float y = radius * cosPhi;
            float z = radius * sinPhi * sinTheta;
            
            float u = (float)j / slices;
            float t = glm::clamp(radius / 20.0f, 0.0f, 1.0f);
            
            float r = u;
            float g = v;
            float b = 0.0f;
            
            vertices.push_back(DirectX::XMFLOAT4(x, y, z, 1.0f));
            vertices.push_back(DirectX::XMFLOAT4(r, g, b, 1.0f));
        }
    }
    
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;
            
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

void KatamatiSphereComponent::InitSphere(float radius, int slices, int stacks)
{
    GenerateSphere(radius, slices, stacks);
    
    std::vector<DirectX::XMFLOAT4> points;
    for (const auto& v : vertices)
    {
        points.push_back(v);
    }
    
    InitPoints(points.data(), static_cast<int>(points.size()), indices.data(), static_cast<int>(indices.size()));
}

void KatamatiSphereComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
    
    InputDevice* inputDevice = GamePtr->GetInputDevice();
    assert(inputDevice);
    
    Player* player = GamePtr->GetPlayer();
    if (!player) return;
    
    if (inputDevice->IsKeyDown(Keys::Q)) { Speed += zoomSpeed * deltaTime; }
    if (inputDevice->IsKeyDown(Keys::E)) { Speed -= zoomSpeed * deltaTime; }
    Speed = std::max(minSpeed, std::min(maxSpeed, Speed));

    const float groundHeight = Radius;
    const bool bIsGrounded = ComponentPosition.y <= groundHeight + 0.001f;

    const bool bJumpKeyDown = inputDevice->IsKeyDown(Keys::F);
    if (bJumpKeyDown && !bJumpKeyWasDown && (bIsGrounded || jumpsUsed < maxJumpCount))
    {
        verticalVelocity = jumpImpulse;
        if (bIsGrounded)
        {
            jumpsUsed = 1;
        }
        else
        {
            jumpsUsed++;
        }
    }
    bJumpKeyWasDown = bJumpKeyDown;
    
    glm::vec3 cameraForward = player->GetOrbitForward();
    glm::vec3 cameraRight = player->GetOrbitRight();
    
    cameraForward.y = 0.0f;
    cameraRight.y = 0.0f;
    
    cameraForward = glm::normalize(cameraForward);
    cameraRight = glm::normalize(cameraRight);
    
    glm::vec3 moveDelta(0.0f, 0.0f, 0.0f);
    if (inputDevice->IsKeyDown(Keys::W))
    {
        moveDelta += cameraForward;
    }
    if (inputDevice->IsKeyDown(Keys::S))
    {
        moveDelta -= cameraForward;
    }
    if (inputDevice->IsKeyDown(Keys::A))
    {
        moveDelta += cameraRight;
    }
    if (inputDevice->IsKeyDown(Keys::D))
    {
        moveDelta -= cameraRight;
    }
    
    if (glm::length(moveDelta) > 0.01f)
    {
        moveDelta = glm::normalize(moveDelta);
        lastMoveDirection = moveDelta;
    }
    
    glm::vec3 oldPosition = ComponentPosition;

    if (glm::length(moveDelta) > 0.01f)
    {
        ComponentPosition += moveDelta * Speed * deltaTime;
    }

    verticalVelocity -= gravityStrength * deltaTime;
    ComponentPosition.y += verticalVelocity * deltaTime;

    if (ComponentPosition.y <= groundHeight)
    {
        ComponentPosition.y = groundHeight;
        verticalVelocity = 0.0f;
        jumpsUsed = 0;
    }

    const glm::vec3 horizontalDisplacement(
        ComponentPosition.x - oldPosition.x,
        0.0f,
        ComponentPosition.z - oldPosition.z
    );
    const float moveDistance = glm::length(horizontalDisplacement);
    AcceptRotation(moveDelta, moveDistance);
    MarkTransformDirty();
    GetWorldMatrix();
}
