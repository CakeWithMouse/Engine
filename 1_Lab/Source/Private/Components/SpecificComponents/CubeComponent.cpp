#include "../../../../Source/Public/Components/SpecificComponents/CubeComponent.h"


CubeComponent::CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) : GameComponent(pos, rot, scale){}

CubeComponent::CubeComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) : GameComponent(pos, rot, scale, color)
{
    
};

inline void CubeComponent::InitCube()
{
    DirectX::XMFLOAT4 points[16]; // 8 вершин * 2 (позиция+цвет)

    for (int i = 0; i < 8; i++)
    {
        points[i * 2] = cubeVertices[i].position;
        points[i * 2 + 1] = cubeVertices[i].color;
    }

    // Инициализируем базовый компонент
    InitPoints(points, 16, cubeIndices, 36);

    std::cout << "Cube initialized with " << 8 << " vertices and " << 36 << " indices\n";
}

void CubeComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
}
