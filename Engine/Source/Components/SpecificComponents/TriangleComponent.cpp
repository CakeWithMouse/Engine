#include <Engine/Components/SpecificComponents/TriangleComponent.h>

#include <iostream>
TriangleComponent::TriangleComponent() : GameComponent()
{
}

void TriangleComponent::InitSquare(DirectX::XMFLOAT4 points[8], int indices[6])
{
    // Просто передаем данные в базовый класс
    InitPoints(points, 8, indices, 6);
    std::cout << "Triangle initialized with 4 vertices and 6 indices\n";
}

void TriangleComponent::InitSquareWithColor(DirectX::XMFLOAT4 points[8], DirectX::XMFLOAT4 color)
{
    DirectX::XMFLOAT4 interleavedPoints[8];
    
    for (int i = 0; i < 4; i++)
    {
        interleavedPoints[i * 2] = points[i * 2];
        interleavedPoints[i * 2 + 1] = color;
    }
    
    int indices[6] = {0, 1, 2, 1, 0, 3};
    InitPoints(interleavedPoints, 8, indices, 6);
    objectColor = color;
}

float TriangleComponent::GetRotationAngle(float totalTime)
{
    return totalTime * rotationSpeed;
}

void TriangleComponent::ConvertToInterleaved(DirectX::XMFLOAT4* sourcePoints, int pointCount, 
                                            DirectX::XMFLOAT4* destPoints)
{
    memcpy(destPoints, sourcePoints, sizeof(DirectX::XMFLOAT4) * pointCount);
}