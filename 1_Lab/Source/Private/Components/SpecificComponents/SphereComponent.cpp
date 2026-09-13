#include "../../../../Source/Public/Components/SpecificComponents/SphereComponent.h"
#include <iostream>
SphereComponent::SphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) 
    : GameComponent(pos, rot, scale) {}

SphereComponent::SphereComponent(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 color) 
    : GameComponent(pos, rot, scale, color) {}

void SphereComponent::GenerateSphere(float radius, int slices, int stacks)
{
    vertices.clear();
    indices.clear();
    
    for (int i = 0; i <= stacks; ++i)
    {
        float phi = glm::pi<float>() * float(i) / float(stacks);
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);
        
        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * glm::pi<float>() * float(j) / float(slices);
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            
            float x = radius * sinPhi * cosTheta;
            float y = radius * cosPhi;
            float z = radius * sinPhi * sinTheta;
            
            float r = (x + radius) / (2 * radius);
            float g = (y + radius) / (2 * radius);
            float b = (z + radius) / (2 * radius);
            
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

void SphereComponent::InitSphere(float radius, int slices, int stacks)
{
    GenerateSphere(radius, slices, stacks);
    
    std::vector<DirectX::XMFLOAT4> points;
    for (const auto& v : vertices)
    {
        points.push_back(v);
    }
    
    InitPoints(points.data(), points.size(), indices.data(), indices.size());
    
    std::cout << "Sphere initialized with " << vertices.size()/2 << " vertices and " 
              << indices.size() << " indices\n";
}

void SphereComponent::Tick(float deltaTime)
{
    GameComponent::Tick(deltaTime);
    MarkTransformDirty();
    if (orbitSpeed != 0.0f)
    {
        currentAngle += orbitSpeed * deltaTime;
        if (currentAngle > DirectX::XM_2PI)
            currentAngle -= DirectX::XM_2PI;
        
        glm::vec3 newPos;
        newPos.x = orbitCenter.x + orbitDistance * cos(currentAngle);
        newPos.z = orbitCenter.z + orbitDistance * sin(currentAngle);
        newPos.y = orbitCenter.y;// + orbitDistance * cos(currentAngle);
        ComponentPosition = newPos;
    }
    for (const auto& Inst : InstanceArray)
    {
        Inst->Tick(deltaTime);
    }
}

SphereComponent* SphereComponent::CreateSphereInstance(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale, glm::vec4 Color)
{
    SphereComponent* NewComponent = new SphereComponent(pos,rot,scale,Color);
    NewComponent->GamePtr = GamePtr;
    ID3D11Device* device = nullptr;
    GamePtr->GetContext()->GetDevice(&device);
    //device->Release();
    InstanceArray.push_back(NewComponent);
    CreateInstanceBuffer(device);
    return NewComponent;
}
