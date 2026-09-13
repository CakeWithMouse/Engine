#pragma once
#include <directxmath.h>

#include "../../includes/GLM-master/glm/vec3.hpp"

class Game;

namespace DirectX
{
    struct XMFLOAT4X4;
}

enum class CameraMode
{
    Free,
    Orbital,
    ThirdPerson
};

class Player
{
public:
    Player();
    void UpdateCamera(float deltaTime);
    void SetGamePointer(Game* game) { GamePtr = game; }

    const DirectX::XMFLOAT4X4& GetViewMatrix() const { return ViewMatrix; }
    const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return ProjectionMatrix; }
    glm::vec3 GetPosition() const { return position; }
    CameraMode GetCameraMode() const { return cameraMode; }

    void SetCameraMode(CameraMode mode)
    {
        cameraMode = mode;
        bCanSwitchMode = false;
    } //
    void SetOrbitTarget(const glm::vec3& target) { orbitTarget = target; }
    void SetPosition(const glm::vec3& pos) { position = pos; }
    void SetRotate(float yaw, float pitch);
    void StopRotate(bool yaw, bool pitch);
    void EnableWASD(bool bEnable) { bCanWASD = bEnable; };

    float GetYaw() const
    {
        float result = cameraMode == CameraMode::Orbital ? orbitYaw : yaw;
        return result;
    }

    glm::vec3 GetForward() const { return free_forward; }
    glm::vec3 GetRight() const { return free_right; }
    glm::vec3 GetUp() const { return free_up; }
    glm::vec3 GetOrbitForward() const { return orbit_forward; }
    glm::vec3 GetOrbitRight() const { return orbit_right; }
    glm::vec3 GetOrbitUp() const { return orbit_up; }

    void SetMinMaximalOrbitRadius(float radiusMin, float radiusMax)
    {
        minimalOrbitRadius = radiusMin;
        maximalOrbitRadius = radiusMax;
    };
    void SetOrbitRadius(float radius) { orbitRadius = radius; }
    void ProcessInput(float deltaTime);
    void Rotate(float yaw, float pitch);
    void Zoom(float delta);

    Game* GamePtr = nullptr;

protected:
    DirectX::XMFLOAT4X4 ViewMatrix;
    DirectX::XMFLOAT4X4 ProjectionMatrix;

private:
    glm::vec3 position{0.0f, 0.0f, -5.0f};

    float nearPlane = 0.1f;
    float farPlane = 10000.0f;

    float yaw = -DirectX::XM_PIDIV2;
    float pitch = 0.0f;

    bool bCanWASD = true;
    bool bCanSwitchMode = true;
    bool bStopRotateYaw = false;
    bool bStopRotatePitch = false;

    //Orbit
    float orbitRadius = maximalOrbitRadius / 4.f;
    float orbitYaw = 0.0f;
    float orbitPitch = 0.0f;
    float mouseSensitivityOrbital = 0.02f;
    float minimalOrbitRadius = 5.0f;
    float maximalOrbitRadius = 100000.0f;
    glm::vec3 orbitTarget{0.0f, 0.0f, 0.0f};
    glm::vec3 orbit_forward;
    glm::vec3 orbit_right;
    glm::vec3 orbit_up;

    CameraMode cameraMode = CameraMode::Free;

    float moveSpeed = 1.0f;
    float lookSpeed = 2.0f;
    float zoomSpeed = 1.0f;
    float mouseSensitivity = 0.02f;

    //free
    glm::vec3 free_forward{0.0f, 0.0f, 1.0f};
    glm::vec3 free_right{1.0f, 0.0f, 0.0f};
    glm::vec3 free_up{0.0f, 1.0f, 0.0f};


    void UpdateThirdPersonCamera(float deltaTime);
    void UpdateFreeCamera(float deltaTime);
    void UpdateOrbitalCamera(float deltaTime);


    void UpdateProjectionMatrix();
    void UpdateViewMatrix();
    void UpdateVectors();
};
