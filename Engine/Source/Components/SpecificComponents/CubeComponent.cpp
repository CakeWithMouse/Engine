#include <Engine/Components/SpecificComponents/CubeComponent.h>

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



CubeComponent::CubeComponent() { InitCube(); }
CubeComponent::CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) : GameComponent(pos, rot, scale){ InitCube(); }

CubeComponent::CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) : GameComponent(pos, rot, scale, color)
{
    InitCube();
};

void CubeComponent::InitCube()
{
    DirectX::XMFLOAT4 points[16]; // 8 вершин * 2 (позиция+цвет)

    for (int i = 0; i < 8; i++)
    {
        points[i * 2] = cubeVertices[i].position;
        points[i * 2 + 1] = cubeVertices[i].color;
    }

    // Инициализируем базовый компонент
    InitPoints(points, 16, cubeIndices, 36);

}

void CubeComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
}
