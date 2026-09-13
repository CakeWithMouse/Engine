#pragma once
#include <iostream>
#include "../../../../Source/Public/Components/GameComponents.h"

namespace
{
    struct CubeVertex
    {
        DirectX::XMFLOAT4 position;
        DirectX::XMFLOAT4 color;
    };

    CubeVertex cubeVertices[] = {
        // Передняя грань
        {DirectX::XMFLOAT4(-0.5f, -0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}, // 0
        {DirectX::XMFLOAT4(0.5f, -0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)}, // 1
        {DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)}, // 2
        {DirectX::XMFLOAT4(-0.5f, 0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f)}, // 3

        // Задняя грань
        {DirectX::XMFLOAT4(-0.5f, -0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f)}, // 4
        {DirectX::XMFLOAT4(0.5f, -0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)}, // 5
        {DirectX::XMFLOAT4(0.5f, 0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f)}, // 6
        {DirectX::XMFLOAT4(-0.5f, 0.5f, -0.5f, 1.0f), DirectX::XMFLOAT4(0.5f, 0.0f, 0.5f, 1.0f)}, // 7
    };

    int cubeIndices[] = {
        0, 1, 2, 0, 2, 3, // Передняя
        4, 6, 5, 4, 7, 6, // Задняя
        4, 0, 3, 4, 3, 7, // Левая
        1, 5, 6, 1, 6, 2, // Правая
        3, 2, 6, 3, 6, 7, // Верхняя
        4, 5, 1, 4, 1, 0 // Нижняя
    };
}

class CubeComponent : public GameComponent
{
public:
    CubeComponent(){};
    CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale);
    CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color);
    
    virtual void InitCube();
    
    void Tick(float deltaTime) override;
    
private:
    GameComponentNames::GeometryType ObjectType =  GameComponentNames::GeometryType::Object3D;
};

