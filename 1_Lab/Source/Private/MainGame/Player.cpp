#include "../../Public/MainGame/Player.h"
#include "../../Public/InputDevice/InputDevice.h"
#include "../../Public/MainGame/BaseGameClass/Game.h"
#include "../../Public/Display/Display.h"

namespace
{
    
}

Player::Player()
{
    yaw = -DirectX::XM_PIDIV2;
    pitch = 0.0f;

    position = glm::vec3(0.0f, 0.0f, -5.0f);

    // Orbit
    orbitTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    orbitRadius = 200.0f;
    orbitYaw = 0.f;
    orbitPitch = -DirectX::XM_PIDIV2 + 0.1f;

    UpdateVectors();
    UpdateProjectionMatrix();
    UpdateViewMatrix();
}

void Player::UpdateCamera(float deltaTime)
{
    ProcessInput(deltaTime);

    switch (cameraMode)
    {
    case CameraMode::Free:
        {
            UpdateFreeCamera(deltaTime);
            break;
        }
    case CameraMode::Orbital:
        {
            UpdateOrbitalCamera(deltaTime);
            break;
        }
    case CameraMode::ThirdPerson:
        {
            // TODO: do=)
            UpdateFreeCamera(deltaTime);
            break;
        }
    }

    UpdateProjectionMatrix();
    UpdateViewMatrix();
}

void Player::SetRotate(float yaw, float pitch)
{
    if (cameraMode == CameraMode::Free)
    {
        this->yaw = yaw;
        this->pitch = pitch; 
    }else if (cameraMode == CameraMode::Orbital)
    {
        this->orbitYaw = yaw;
        this->orbitPitch = pitch; 
    }
    
}

void Player::StopRotate(bool yaw, bool pitch)
{
    bStopRotateYaw = yaw;
    bStopRotatePitch  = pitch;
}

void Player::ProcessInput(float deltaTime)
{
    if (!GamePtr) { return; }
    InputDevice* input = GamePtr->GetInputDevice();
    if (!input) { return; }

    if (bCanSwitchMode)
    {
        if (input->IsKeyDown(Keys::F1))
        {
            cameraMode = CameraMode::Free;
        }
        if (input->IsKeyDown(Keys::F2))
        {
            cameraMode = CameraMode::Orbital;
        }
    }

    if (cameraMode == CameraMode::Free || cameraMode == CameraMode::ThirdPerson)
    {
        //glm::vec2 mouseDelta = {0.f, 0.f}; //input->MouseOffset;
        glm::vec2 mouseDelta = input->MouseOffset; //input->MouseOffset;

        yaw -= mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;

        pitch = std::max(-DirectX::XM_PIDIV2 + 0.01f, std::min(DirectX::XM_PIDIV2 - 0.01f, pitch));
        UpdateVectors();
    }
    else if (cameraMode == CameraMode::Orbital)
    {
        glm::vec2 mouseDelta = input->MouseOffset;
        
        if (bStopRotateYaw)
        {
            orbitYaw += mouseDelta.x * mouseSensitivityOrbital;
        }
        if (bStopRotatePitch)
        {
            orbitPitch += mouseDelta.y * mouseSensitivityOrbital;
        }
        
        const float AngelOffsetByKey = 1.f * mouseSensitivityOrbital * 5.f * deltaTime;
        if (bCanWASD)
        {
            if (input->IsKeyDown(Keys::W))
            {
                orbitPitch -= AngelOffsetByKey;
            }
            if (input->IsKeyDown(Keys::S))
            {
                orbitPitch += AngelOffsetByKey;
            }
            if (input->IsKeyDown(Keys::A))
            {
                orbitYaw -= AngelOffsetByKey;
            }
            if (input->IsKeyDown(Keys::D))
            {
                orbitYaw += AngelOffsetByKey;
            }
        }
        
        orbitPitch = std::max(-DirectX::XM_PIDIV2 + 0.1f, std::min(DirectX::XM_PIDIV2 - 0.1f, orbitPitch));
        if (orbitYaw > DirectX::XM_2PI) orbitYaw -= DirectX::XM_2PI;
        if (orbitYaw < 0) orbitYaw += DirectX::XM_2PI;
    }

    // The wheel is an event quantity (accumulated per frame), so it is not scaled by frame time.
    // 0.13 keeps the step per notch equal to the old per-frame behaviour.
    constexpr float WheelStep = 0.13f;
    int wheelDelta = input->MouseWheelDelta;
    if (wheelDelta != 0)
    {
        if (cameraMode == CameraMode::Orbital)
        {
            orbitRadius -= wheelDelta * zoomSpeed * WheelStep;
            orbitRadius = std::max(minimalOrbitRadius, std::min(maximalOrbitRadius, orbitRadius));
        }
        else
        {
            moveSpeed += wheelDelta * WheelStep;
            moveSpeed = std::max(2.0f, std::min(50.0f, moveSpeed));
        }
    }
}

void Player::UpdateThirdPersonCamera(float deltaTime)
{
    InputDevice* input = GamePtr->GetInputDevice();
    if (!input) return;

    glm::vec3 moveDelta(0.0f, 0.0f, 0.0f);

    if (bCanWASD)
    {
        if (input->IsKeyDown(Keys::W)) { moveDelta += free_forward; }
        if (input->IsKeyDown(Keys::S)) { moveDelta -= free_forward; }
        if (input->IsKeyDown(Keys::A)) { moveDelta += free_right; }
        if (input->IsKeyDown(Keys::D)) { moveDelta -= free_right; }
        //if (input->IsKeyDown(Keys::E)) { moveDelta -= free_up; }
        //if (input->IsKeyDown(Keys::Q)) { moveDelta += free_up; }
    }
    
    // Нормализуем диагональное движение
    if (moveDelta.x != 0 || moveDelta.y != 0 || moveDelta.z != 0)
    {
        moveDelta = glm::normalize(moveDelta);
    }

    moveDelta.z =0.f;
    position += moveDelta * moveSpeed * deltaTime;
}

void Player::UpdateFreeCamera(float deltaTime)
{
    InputDevice* input = GamePtr->GetInputDevice();
    if (!input) return;

    glm::vec3 moveDelta(0.0f, 0.0f, 0.0f);

    if (bCanWASD)
    {
        if (input->IsKeyDown(Keys::W)) { moveDelta += free_forward; }
        if (input->IsKeyDown(Keys::S)) { moveDelta -= free_forward; }
        if (input->IsKeyDown(Keys::A)) { moveDelta += free_right; }
        if (input->IsKeyDown(Keys::D)) { moveDelta -= free_right; }
        if (input->IsKeyDown(Keys::E)) { moveDelta -= free_up; }
        if (input->IsKeyDown(Keys::Q)) { moveDelta += free_up; }
    }
    

    // Нормализуем диагональное движение
    if (moveDelta.x != 0 || moveDelta.y != 0 || moveDelta.z != 0)
    {
        moveDelta = glm::normalize(moveDelta);
    }

    position += moveDelta * moveSpeed * deltaTime;
}

void Player::UpdateOrbitalCamera(float deltaTime)
{
    float x = orbitTarget.x + orbitRadius * cos(orbitPitch) * cos(orbitYaw);
    float y = orbitTarget.y + orbitRadius * sin(orbitPitch);
    float z = orbitTarget.z + orbitRadius * cos(orbitPitch) * sin(orbitYaw);

    position = glm::vec3(x, y, z);

    orbit_forward = glm::normalize(orbitTarget - position);
    orbit_right = glm::normalize(glm::cross(orbit_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    orbit_up = glm::normalize(glm::cross(orbit_right, orbit_forward));
}

void Player::UpdateProjectionMatrix()
{
    using namespace DirectX;

    if (!GamePtr || !GamePtr->GetDisplay()) { return; }

    const int height = GamePtr->GetDisplay()->GetHeight();
    if (height <= 0) { return; }
    const float aspect = GamePtr->GetDisplay()->GetWidth() / static_cast<float>(height);

    const XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(GetFovY(), aspect, nearPlane, farPlane);
    const XMMATRIX projTransposed = XMMatrixTranspose(projMatrix);
    XMStoreFloat4x4(&ProjectionMatrix, projTransposed);
}

void Player::UpdateViewMatrix()
{
    using namespace DirectX;

    if (glm::length(free_forward) < 0.001f)
    {
        free_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    XMVECTOR Eye, At, Up;
    switch (cameraMode)
    {
    case CameraMode::Free:
        {
            Eye = XMVectorSet(position.x, position.y, position.z, 0.0f);
            At = XMVectorSet(position.x + free_forward.x, position.y + free_forward.y, position.z + free_forward.z, 0.0f);
            Up = XMVectorSet(free_up.x, free_up.y, free_up.z, 0.0f);
            break;
        }

    case CameraMode::Orbital:
        {
            Eye = XMVectorSet(position.x, position.y, position.z, 0.0f);
            At = XMVectorSet(position.x + orbit_forward.x, position.y + orbit_forward.y, position.z + orbit_forward.z,
                             0.0f);
            Up = XMVectorSet(orbit_up.x, orbit_up.y, orbit_up.z, 0.0f);
            break;
            //UpdateOrbitalCamera(deltaTime);
            //break;
        }

    case CameraMode::ThirdPerson:
        {
            Eye = XMVectorSet(position.x, position.y, position.z, 0.0f);
            At = XMVectorSet(position.x + free_forward.x, position.y + free_forward.y, position.z + free_forward.z, 0.0f);
            Up = XMVectorSet(free_up.x, free_up.y, free_up.z, 0.0f);
            break;
        }
    }


    XMMATRIX viewMatrix = XMMatrixLookAtLH(Eye, At, Up);
    XMMATRIX viewTransposed = XMMatrixTranspose(viewMatrix);
    XMStoreFloat4x4(&ViewMatrix, viewTransposed);
}

void Player::UpdateVectors()
{
    free_forward.x = cos(yaw) * cos(pitch);
    free_forward.y = sin(pitch);
    free_forward.z = sin(yaw) * cos(pitch);

    free_forward = glm::normalize(free_forward);

    free_right = glm::normalize(glm::cross(free_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    free_up = glm::normalize(glm::cross(free_right, free_forward));
}

void Player::Rotate(float yawDelta, float pitchDelta)
{
    yaw += yawDelta;
    pitch += pitchDelta;
    pitch = std::max(-DirectX::XM_PIDIV2 + 0.01f, std::min(DirectX::XM_PIDIV2 - 0.01f, pitch));
    UpdateVectors();
}

void Player::Zoom(float delta)
{
    if (cameraMode == CameraMode::Orbital)
    {
        orbitRadius -= delta;
        orbitRadius = std::max(minimalOrbitRadius, std::min(maximalOrbitRadius, orbitRadius));
    }
}
