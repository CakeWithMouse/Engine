#pragma once

#include <glm/vec4.hpp>
#include <Engine/Resources/Paths.h>

#include <filesystem>
#include <string>

class BaseResources
{
public:
    // Runtime skeleton: no actual GPU resources are created here yet.
    bool Initialize();
    bool DeInitialize();
private:
    bool initialized = false;
public:  
    static constexpr glm::vec4 White{1.0f, 1.0f, 1.f, 1.f};
    static constexpr glm::vec4 Red{0.5f, 0.0f, 0.f, 1.f};
    static constexpr glm::vec4 Blue{0.0f, 0.0f, 1.f, 1.f};
    static constexpr glm::vec4 Green{0.0f, 0.7f, 0.f, 1.f};
    static constexpr glm::vec4 Yellow{1.0f, 0.2f, 0.f, 1.f};
    static constexpr glm::vec4 Yellow_a{0.7f, 0.7f, 0.f, 0.4f};

    std::string BaseMaterial = EnginePaths::Shader("RotatedFigure.hlsl").generic_u8string();
    std::string SunMaterial = EnginePaths::Shader("BaseSphereFigure.hlsl").generic_u8string();
    std::string LinesMaterial = EnginePaths::Shader("BaseLines.hlsl").generic_u8string();
    std::string InstanceMaterial = EnginePaths::Shader("InstanceSphereFigure.hlsl").generic_u8string();
    std::string FBXMaterial = EnginePaths::Shader("BaseFBX.hlsl").generic_u8string();
    std::string ReflectiveSphereMaterial = EnginePaths::Shader("ReflectiveSphere.hlsl").generic_u8string();
    std::string ReflectiveFBXMaterial = EnginePaths::Shader("ReflectiveFBX.hlsl").generic_u8string();
    std::string SkyboxMaterial = EnginePaths::Shader("Skybox.hlsl").generic_u8string();

    std::string BaseVertex = EnginePaths::Shader("RotatedFigure.hlsl").generic_u8string();
    std::string SphereVertex = EnginePaths::Shader("BaseSphereFigure.hlsl").generic_u8string();
    std::string InstanceSphereVertex = EnginePaths::Shader("InstanceSphereFigure.hlsl").generic_u8string();
    std::string FBXVertex = EnginePaths::Shader("BaseFBX.hlsl").generic_u8string();
    std::string ReflectiveSphereVertex = EnginePaths::Shader("ReflectiveSphere.hlsl").generic_u8string();
    std::string ReflectiveFBXVertex = EnginePaths::Shader("ReflectiveFBX.hlsl").generic_u8string();
    std::string SkyboxVertex = EnginePaths::Shader("Skybox.hlsl").generic_u8string();
    
    std::wstring ReadEnvironmentVariable(const wchar_t* name);

    // Historical name: now returns the engine content directory (containing Shaders).
    // Set EnginePaths::SetContentRoot before constructing BaseResources.
    std::filesystem::path FindProjectDirectory();

    // Engine content only; game assets have a separate root owned by the application.
    std::string ResolveContentRoot();
};
