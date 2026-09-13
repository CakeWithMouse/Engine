#pragma once
#include "../GameComponents.h"

namespace Square
{
    extern DirectX::XMFLOAT4 SquarePoints[2][8];
    extern int SquareIndices[6];
}

class TriangleComponent : public GameComponent
{
public:
    TriangleComponent();
    
    void InitSquare(DirectX::XMFLOAT4 points[8], int indices[6]);
    void InitSquareWithColor(DirectX::XMFLOAT4 points[8], DirectX::XMFLOAT4 color);
    
    virtual float GetRotationAngle(float totalTime) override;
    virtual int GetIndexCount() override { return 6; }
    
    void SetRotationSpeed(float speed) { rotationSpeed = speed; }
    void SetColor(const DirectX::XMFLOAT4& color) { objectColor = color; }
    
private:
    float rotationSpeed = 1.0f;
    DirectX::XMFLOAT4 objectColor = {1.0f, 1.0f, 1.0f, 1.0f};
    
    void ConvertToInterleaved(DirectX::XMFLOAT4* sourcePoints, int pointCount, 
                              DirectX::XMFLOAT4* destPoints);
    GameComponentNames::GeometryType ObjectType =  GameComponentNames::GeometryType::Object2D;
};
