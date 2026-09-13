#include "../../../Public/Components/SpecificComponents/TriangleComponent.h"

#include <iostream>
namespace Square
{
    DirectX::XMFLOAT4 SquarePoints[2][8] = {
        {
            DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
            DirectX::XMFLOAT4(-0.5f, -0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(0.5f, -0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
            DirectX::XMFLOAT4(-0.5f, 0.5f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        },
        {
            DirectX::XMFLOAT4(0.25f, 0.25f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f),
            DirectX::XMFLOAT4(-0.25f, -0.25f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(0.25f, -0.25f, 0.5f, 1.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
            DirectX::XMFLOAT4(-0.25f, 0.25f, 0.5f, 1.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        }
    };
    int SquareIndices[6] = {0, 1, 2, 1, 0, 3};
}

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
    
    InitPoints(interleavedPoints, 8, Square::SquareIndices, 6);
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