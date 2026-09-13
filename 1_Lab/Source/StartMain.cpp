// MySuper3DApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Public/MainGame/BaseGameClass/Game.h"
#include <windows.h>
#include <WinUser.h>
#include <wrl.h>
#include <iostream>
#include <d3d.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <chrono>
#include <algorithm>

#include "Public/Components/Light/CubeMap.h"
#include "Public/Components/SpecificComponents/CubeComponent.h"
#include "Public/Components/SpecificComponents/FBXComponent.h"
#include "Public/Components/SpecificComponents/ParticleSystemComponent.h"
#include "Public/Components/Light/PointLightComponent.h"
#include "Public/Components/SpecificComponents/SphereComponent.h"
#include "Public/Components/SpecificComponents/TriangleComponent.h"
#include "Public/MainGame/BaseGameClass/BaseGame.h"
#include "Public/MainGame/SpecialGameClasses/Katamari/KatamariGame.h"
#include "Public/MainGame/SpecialGameClasses/Katamari/KatamatiSphereComponent.h"
#include "Public/MainGame/SpecialGameClasses/PingPong/Ball.h"
#include "Public/MainGame/SpecialGameClasses/PingPong/PongGame.h"
#include "Public/MainGame/SpecialGameClasses/PingPong/Rocket.h"
#include "Public/MainGame/SpecialGameClasses/PingPong/Wall.h"
#include "Public/MainGame/SpecialGameClasses/SunSystem/SunGame.h"


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

void FirstLabStart_a();
void FirstLabStart_b();
void SecondLabStart();
void ThirdLabStart(std::string Path);
void ForthLabStart(std::string Path);


namespace main_lib
{
    //Type
    constexpr GameType CurrentGameType = ThirdLab;
    constexpr RenderType RenderingType = Deffered;
    constexpr bool Computer = false;
    
    //Light
    constexpr float SunDistanceMult = 1.f;
    constexpr glm::vec3 SunDirectional = glm::vec3(-0.35f, -0.85f, -0.2f) * SunDistanceMult;
    constexpr glm::vec3 SunLightColor = glm::vec3(0.95f, 0.93f, 0.90f);
    constexpr float SunItensity = 0.40f;
    constexpr float SkyboxPower = 0.8f;
    
    //Shadows
    constexpr float ShadowDist = 14000.f;
    constexpr bool ShadowCasting = true;
    
    std::string NoteBookPath = "E:/ComputerGraphic";
    std::string ComputerPath = "O:/ITMO/ComputerGraphic";
    
    constexpr glm::vec4 White{1.0f, 1.0f, 1.f, 1.f};
    constexpr glm::vec4 Red{0.5f, 0.0f, 0.f, 1.f};
    constexpr glm::vec4 Blue{0.0f, 0.0f, 1.f, 1.f};
    constexpr glm::vec4 Green{0.0f, 0.7f, 0.f, 1.f};
    constexpr glm::vec4 Yellow{1.0f, 0.2f, 0.f, 1.f};
    constexpr glm::vec4 Yellow_a{0.7f, 0.7f, 0.f, 0.4f};

    std::string BaseMaterial = "Source/Shaders/RotatedFigure.hlsl";
    std::string SunMaterial = "Source/Shaders/BaseSphereFigure.hlsl";
    std::string LinesMaterial = "Source/Shaders/BaseLines.hlsl";
    std::string InstanceMaterial = "Source/Shaders/InstanceSphereFigure.hlsl";
    std::string FBXMaterial = "Source/Shaders/BaseFBX.hlsl";
    std::string ReflectiveSphereMaterial = "Source/Shaders/ReflectiveSphere.hlsl";
    std::string ReflectiveFBXMaterial = "Source/Shaders/ReflectiveFBX.hlsl";
    std::string SkyboxMaterial = "Source/Shaders/Skybox.hlsl";

    std::string BaseVertex = "Source/Shaders/RotatedFigure.hlsl";
    std::string SphereVertex = "Source/Shaders/BaseSphereFigure.hlsl";
    std::string InstanceSphereVertex = "Source/Shaders/InstanceSphereFigure.hlsl";
    std::string FBXVertex = "Source/Shaders/BaseFBX.hlsl";
    std::string ReflectiveSphereVertex = "Source/Shaders/ReflectiveSphere.hlsl";
    std::string ReflectiveFBXVertex = "Source/Shaders/ReflectiveFBX.hlsl";
    std::string SkyboxVertex = "Source/Shaders/Skybox.hlsl";
}

using namespace main_lib;

int main()
{
    std::string ProjectPath = Computer ? ComputerPath : NoteBookPath;
    
    switch (CurrentGameType)
    {
    case FirstLabTriangles:
        {
            FirstLabStart_a();
            break;
        }
    case FirstLabCubes:
        {
            FirstLabStart_b();
            break;
        }
    case SecondLab:
        {
            SecondLabStart();
            break;
        }
    case ThirdLab:
        {
            ThirdLabStart(ProjectPath);
            break;
        }
    case ForthLab:
        {
            ForthLabStart(ProjectPath);
            break;
        }
    default: break;
    }
}



void FirstLabStart_a()
{
    BaseGame* MainGame = new BaseGame();
    MainGame->Initialize();
    MainGame->SetGameType(CurrentGameType);

    TriangleComponent* square1 = new TriangleComponent();
    square1->InitSquare(Square::SquarePoints[0], Square::SquareIndices);
    square1->SetRotationSpeed(1.0f);
    MainGame->RegisterComponent("1", square1);

    TriangleComponent* square2 = new TriangleComponent();
    square2->InitSquare(Square::SquarePoints[1], Square::SquareIndices);
    square2->SetRotationSpeed(-1.5f);
    MainGame->RegisterComponent("2", square2);

    MainGame->StartGame();
}

void FirstLabStart_b()
{
    BaseGame* MainGame = new BaseGame();
    MainGame->Initialize();
    MainGame->SetGameType(CurrentGameType);

    CubeComponent* Cube = new CubeComponent();
    Cube->InitCube();
    MainGame->RegisterComponent("1", Cube);

    MainGame->StartGame();
}

void SecondLabStart()
{
    PongGame* MainGame = new PongGame();
    MainGame->Initialize();
    MainGame->SetGameType(CurrentGameType);

    glm::vec3 ComponentPositionLeft{-4.5f, 0.0f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{0.2f, 2.5f, 1.f};
    RocketComponent* Cube = new
        RocketComponent(ComponentPositionLeft, ComponentRotation, ComponentScale, White);
    Cube->InitCube();
    Cube->SetRocketType(RocketType::Left);
    Cube->SetCollision(true);
    MainGame->RegisterComponent("5", Cube);

    glm::vec3 ComponentPositionRight{4.5f, 0.0f, 0.0f};
    RocketComponent* Cube2 = new RocketComponent(ComponentPositionRight, ComponentRotation, ComponentScale,
                                                 White);
    Cube2->InitCube();
    Cube2->SetCollision(true);
    Cube->SetRocketType(RocketType::Right);
    MainGame->RegisterComponent("6", Cube2);

    glm::vec3 ComponentPositionCenter{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotationBall{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScaleBall{0.3f, 0.3f, 1.f};
    BallComponent* Ball = new BallComponent(ComponentPositionCenter, ComponentRotationBall, ComponentScaleBall,
                                            Blue);
    Ball->InitCube();
    MainGame->RegisterComponent("PongBall", Ball);

    glm::vec3 ComponentScaleWall{0.08f, 15.f, 1.f};
    CubeComponent* WallCenter = new CubeComponent(ComponentPositionCenter, ComponentRotation,
                                                  ComponentScaleWall,
                                                  White);
    WallCenter->InitCube();
    MainGame->RegisterComponent("4", WallCenter);

    glm::vec3 ComponentPositionCenterUp{0.f, 4.f, 1.f};
    glm::vec3 ComponentScaleWall2{0.12f, 1.f, 1.f};
    CubeComponent* WallCenterUp = new CubeComponent(ComponentPositionCenterUp, ComponentRotation,
                                                    ComponentScaleWall2,
                                                    Green);
    WallCenterUp->InitCube();
    MainGame->RegisterComponent("7", WallCenterUp);

    glm::vec3 ComponentPositionCenterDown{0.f, -4.f, 1.f};
    CubeComponent* WallCenterDown = new CubeComponent(ComponentPositionCenterDown, ComponentRotation,
                                                      ComponentScaleWall2,
                                                      Green);
    WallCenterDown->InitCube();
    MainGame->RegisterComponent("8", WallCenterDown);

    glm::vec3 ComponentPositionCenterWall{0.f, 0.f, 1.f};
    CubeComponent* WallCenterWall = new CubeComponent(ComponentPositionCenterWall, ComponentRotation,
                                                      ComponentScaleWall2,
                                                      Green);
    WallCenterWall->InitCube();
    MainGame->RegisterComponent("9", WallCenterWall);

    glm::vec3 ComponentScaleWallEnd{0.1f, 15.f, 1.f};
    glm::vec3 ComponentPositionLefWallt{-5.2f, 0.0f, 0.0f};
    Wall* WallLeft = new Wall(ComponentPositionLefWallt, ComponentRotation, ComponentScaleWallEnd,
                              Red);
    WallLeft->InitCube();
    WallLeft->SetCollision(true);
    WallLeft->SetWallType(WallType::LeftWall);
    MainGame->RegisterComponent("1", WallLeft);
    glm::vec3 ComponentPositionRightWall{5.2f, 0.0f, 0.0f};
    Wall* WallRight = new Wall(ComponentPositionRightWall, ComponentRotation, ComponentScaleWallEnd,
                               Red);
    WallRight->InitCube();
    WallRight->SetCollision(true);
    WallRight->SetWallType(WallType::RightWall);
    MainGame->RegisterComponent("2", WallRight);

    MainGame->StartGame();
}

constexpr int ActeroidCount = 500;
constexpr int ActeroidSunCount = 900;
constexpr int StarCount = 500;
constexpr float StartsItensity = 2.1f;
void ThirdLabStart(std::string Path)
{
    SunGame* MainGame = new SunGame();
    MainGame->Initialize();
    MainGame->SetDirectionalLight(SunDirectional,SunLightColor,SunItensity);
    //MainGame->SetShadowSettings(ShadowCasting, ShadowDist);
    MainGame->SetGameType(CurrentGameType);
    MainGame->SetRenderingType(RenderingType);
    MainGame->CreateCubeMapFromFiles("MySky", {
                                         Path + "/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg"
                                     });

    SkyboxComponent* Skybox = new SkyboxComponent(glm::vec3(9000.0f), glm::vec4(1.0f));

    Skybox->InitCube();
    Skybox->SetCubeMap(MainGame->GetCubeMap("MySky"));
    Skybox->SetReflectionSettings(SkyboxPower, 100.0f);
    //MainGame->RegisterComponent("Skybox", Skybox, SkyboxMaterial, SkyboxVertex);

    PointLightComponent* MainWarmLight = new PointLightComponent(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.85f, 0.6f),
        6.2f,
        5600.0f
    );
    MainGame->RegisterComponent("PointLight_MainWarm", MainWarmLight);

    PointLightComponent* BlueFillLight = new PointLightComponent(
        glm::vec3(-450.0f, 80.0f, 320.0f),
        glm::vec3(0.2f, 0.45f, 1.0f),
        1.25f,
        1800.0f
    );
    MainGame->RegisterComponent("PointLight_BlueFill", BlueFillLight);

    PointLightComponent* RedRimLight = new PointLightComponent(
        glm::vec3(520.0f, 60.0f, -420.0f),
        glm::vec3(1.0f, 0.25f, 0.2f),
        1.1f,
        1700.0f
    );
    MainGame->RegisterComponent("PointLight_RedRim", RedRimLight);

    glm::vec3 sunPos{0.0f, 0.0f, 0.0f};
    glm::vec3 sunRot{0.f, 0.f, 0.f};

    float sunRadius = 2.0f;
    glm::vec3 sunScale{sunRadius, sunRadius, sunRadius};

    struct PlanetData
    {
        std::string name;
        float distance;
        float radius;
        float speed;
        glm::vec4 color;
        bool hasAtmosphere;
    };
    float dist_mult = 50.f;
    float speed_mult = 0.1f;
    float radius_mult = 2.f;
    std::vector<PlanetData> planets = {
        {"Mercury", 6.0f * dist_mult, 0.4f * radius_mult, 3.0f * speed_mult, glm::vec4(0.8f, 0.6f, 0.4f, 1.0f), false},
        {"Venus", 8.0f * dist_mult, 0.5f * radius_mult, 2.2f * speed_mult, glm::vec4(1.0f, 0.7f, 0.4f, 1.0f), true},
        {"Earth", 10.0f * dist_mult, 0.55f * radius_mult, 1.8f * speed_mult, glm::vec4(0.2f, 0.5f, 0.9f, 1.0f), true},
        {"Mars", 12.5f * dist_mult, 0.45f * radius_mult, 1.4f * speed_mult, glm::vec4(0.9f, 0.3f, 0.2f, 1.0f), false},
        {"Jupiter", 18.0f * dist_mult, 1.0f * radius_mult, 0.9f * speed_mult, glm::vec4(0.8f, 0.7f, 0.6f, 1.0f), false},
        {"Saturn", 23.0f * dist_mult, 0.9f * radius_mult, 0.7f * speed_mult, glm::vec4(0.9f, 0.8f, 0.5f, 1.0f), true},
        {"Uranus", 28.0f * dist_mult, 0.7f * radius_mult, 0.5f * speed_mult, glm::vec4(0.5f, 0.8f, 0.8f, 1.0f), false},
        {"Neptune", 33.0f * dist_mult, 0.7f * radius_mult, 0.4f * speed_mult, glm::vec4(0.2f, 0.3f, 0.8f, 1.0f), false}
    };

    // Colnse
    float sunRadius2 = 1.5f;
    glm::vec3 sunScale2{sunRadius2, sunRadius2, sunRadius2};
    SphereComponent* Sun = new SphereComponent(sunPos, sunRot, sunScale2, glm::vec4(200.0f, 100.8f, 100.6f, 100.0f));
    Sun->InitSphere(60.0f, 100, 100);
    Sun->SetOrbitSpeed(0.0f);
    MainGame->RegisterComponent("Sun", Sun, {}, SunMaterial);

    //float sunRadius2 = 1.5f;
    //glm::vec3 sunScale2{sunRadius2, sunRadius2, sunRadius2};
    /*SphereComponent* Sky = new SphereComponent(sunPos, sunRot, sunScale2, glm::vec4(0.0f, 0.1f, 0.6f, 1.0f));
    Sky->InitSphere(9000.0f, 100, 100);
    Sky->SetOrbitSpeed(0.0f);
    MainGame->RegisterComponent("Sky", Sky, {}, SunMaterial);*/

    SphereComponent* SunAtmosphere = new SphereComponent(sunPos, sunRot, glm::vec3(sunRadius + 0.5f),
                                                         glm::vec4(1.0f, 0.5f, 0.1f, 0.3f));
    SunAtmosphere->InitSphere(65.0f, 80, 80);
    SunAtmosphere->SetOrbitSpeed(0.0f);
    MainGame->RegisterComponent("SunAtmosphere", SunAtmosphere, {}, SunMaterial);

    ParticleSystemComponent* SolarParticles = new ParticleSystemComponent(
        glm::vec3(0.0f, -500.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(20.0f, 20.0f, 20.0f),
        glm::vec4(1.0f, 0.55f, 0.20f, 1.0f)
    );
    SolarParticles->SetParticleCount(10000);
    SolarParticles->SetBaseLifetime(8.4f);
    SolarParticles->SetSpreadRadius(0.0f);
    SolarParticles->SetRadialSpeed(4.0f);
    SolarParticles->SetMaxDistance(1000.0f);
    SolarParticles->SetAcceleration(glm::vec3(0.0f, 50.0f, 0.0f));
    SolarParticles->SetBaseSize(0.18f);
    SolarParticles->SetBrightness(3.5f);
    SolarParticles->SetRandomness(0.85f, 1.65f);
    MainGame->RegisterComponent("GPU_Particles", SolarParticles);

    SphereComponent* FirstAsteroid = nullptr;
    for (int i = 0; i < ActeroidSunCount; ++i)
    {
        float asteroidSize_mul = 3.f;
        float asteroiddis_mul = 5.f;
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float min_dist = 85.f;
        float max_dist = 160.f;
        float distance = min_dist * asteroiddis_mul + rand() % static_cast<int>(max_dist - min_dist);
        float yOffset = ((rand() % 200) - 100) / 50.0f;

        glm::vec3 asteroidPos{
            distance * cos(angle),
            yOffset,
            distance * sin(angle)
        };
        float asteroidSize = 0.08f * asteroidSize_mul + (rand() % 15 * asteroidSize_mul) / 100.0f;
        float OrbitSpeed_mul = 0.05f;
        glm::vec3 asteroidScale{asteroidSize, asteroidSize, asteroidSize};

        if (i == 0)
        {
            asteroidScale.x *= 50;
            asteroidScale.y *= 60;
            SphereComponent* Asteroid = new SphereComponent(asteroidPos, sunRot, asteroidScale,
                                                            glm::vec4(0.3f, 0.25f, 0.2f, 1.0f));
            Asteroid->InitSphere(asteroidSize * 8.0f, 15, 15);
            Asteroid->SetOrbitSpeed((1.f + (rand() % 300) / 100.f) * OrbitSpeed_mul);
            Asteroid->SetOrbitDistance(distance);
            Asteroid->SetOrbitCenter(sunPos);
            Asteroid->SetParent(Sun);
            FirstAsteroid = Asteroid;
            MainGame->RegisterComponent("AsteroidSun" + std::to_string(i), Asteroid, InstanceSphereVertex,
                                        InstanceMaterial);
            continue;
        }
        SphereComponent* NewInstance = FirstAsteroid->CreateSphereInstance(
            asteroidPos, sunRot, asteroidScale, glm::vec4(0.48f, 0.40f, 0.33f, 1.0f));
        NewInstance->InitSphere(asteroidSize * 8.0f, 15, 15);
        NewInstance->SetOrbitSpeed((1.f + (rand() % 300) / 100.f) * OrbitSpeed_mul);
        NewInstance->SetOrbitDistance(distance);
        NewInstance->SetOrbitCenter(sunPos);
        NewInstance->SetParent(Sun);
    }
    //SphereComponent* SecondAsteroid = nullptr;
    for (int i = 0; i < ActeroidSunCount; ++i)
    {
        float asteroidSize_mul = 3.f;
        float asteroiddis_mul = 5.f;
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float min_dist = 200.f;
        float max_dist = 500.f;
        float distance = min_dist * asteroiddis_mul + rand() % static_cast<int>(max_dist - min_dist);
        float yOffset = ((rand() % 200) - 100) / 50.0f;
        float OrbitSpeed_mul = 0.05f;

        glm::vec3 asteroidPos{
            distance * cos(angle),
            yOffset,
            distance * sin(angle)
        };
        float asteroidSize = 0.08f * asteroidSize_mul + (rand() % 15 * asteroidSize_mul) / 100.0f;
        glm::vec3 asteroidScale{asteroidSize, asteroidSize, asteroidSize};

        if (i == 0 && FirstAsteroid == nullptr)
        {
            asteroidScale.x *= 50;
            asteroidScale.y *= 60;
            SphereComponent* Asteroid = new SphereComponent(asteroidPos, sunRot, asteroidScale,
                                                            glm::vec4(0.3f, 0.25f, 0.2f, 1.0f));
            Asteroid->InitSphere(asteroidSize * 8.0f, 15, 15);
            Asteroid->SetOrbitSpeed((1.f + (rand() % 300) / 100.f) * OrbitSpeed_mul);
            Asteroid->SetOrbitDistance(distance);
            Asteroid->SetOrbitCenter(sunPos);
            Asteroid->SetParent(Sun);
            FirstAsteroid = Asteroid;
            MainGame->RegisterComponent("AsteroidSun2" + std::to_string(i), Asteroid, InstanceSphereVertex,
                                        InstanceMaterial);
            continue;
        }
        SphereComponent* NewInstance = FirstAsteroid->CreateSphereInstance(
            asteroidPos, sunRot, asteroidScale, glm::vec4(0.48f, 0.40f, 0.33f, 1.0f));
        NewInstance->InitSphere(asteroidSize * 8.0f, 15, 15);
        NewInstance->SetOrbitSpeed((1.f + (rand() % 300) / 100.f) * OrbitSpeed_mul);
        NewInstance->SetOrbitDistance(distance);
        NewInstance->SetOrbitCenter(sunPos);
        NewInstance->SetParent(Sun);
    }

    SphereComponent* Uranus = nullptr;
    // Planeti
    for (size_t i = 0; i < planets.size(); ++i)
    {
        const auto& data = planets[i];

        float startAngle = (i * 360.0f / planets.size()) * 3.14159f / 180.0f;
        glm::vec3 planetPos{
            data.distance * cos(startAngle),
            0.0f,
            data.distance * sin(startAngle)
        };

        glm::vec3 planetScale{data.radius, data.radius, data.radius};

        SphereComponent* Planet = new SphereComponent(planetPos, sunRot, planetScale, data.color);
        Planet->InitSphere(data.radius * 15.0f, 80, 80);
        Planet->SetOrbitSpeed(data.speed);
        Planet->SetOrbitDistance(data.distance);
        Planet->SetOrbitCenter(sunPos);
        Planet->SetCubeMap(MainGame->GetCubeMap("MySky"));
        Planet->SetReflectionSettings(0.88f, 1.2f);
        if (data.name == "Uranus")
        {
            Uranus = Planet;
        }
        MainGame->RegisterComponent(data.name, Planet, ReflectiveSphereMaterial, ReflectiveSphereVertex);

        // Атмосфера планеты
        if (data.hasAtmosphere)
        {
            SphereComponent* Atmosphere = new SphereComponent(planetPos, sunRot,
                                                              glm::vec3(data.radius + 0.1f),
                                                              glm::vec4(1.0f, 1.0f, 1.0f, 0.25f));
            Atmosphere->InitSphere((data.radius + 0.1f) * 15.0f, 60, 60);
            Atmosphere->SetOrbitSpeed(data.speed);
            Atmosphere->SetOrbitDistance(data.distance);
            Atmosphere->SetOrbitCenter(sunPos);
            MainGame->RegisterComponent(data.name + "Atmosphere", Atmosphere, {}, SunMaterial);
        }
    }

    for (int i = 0; i < ActeroidCount; ++i)
    {
        float asteroidSize_mul = 2.f;
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float distance = 14.0f * asteroidSize_mul + (rand() % 300) / 100.0f;
        float yOffset = ((rand() % 200) - 100) / 50.0f;

        glm::vec3 asteroidPos{
            distance * cos(angle),
            yOffset,
            distance * sin(angle)
        };
        float asteroidSize = 0.08f * asteroidSize_mul + (rand() % 15 * asteroidSize_mul) / 100.0f;
        glm::vec3 asteroidScale{asteroidSize, asteroidSize, asteroidSize};

        SphereComponent* Asteroid = new SphereComponent(asteroidPos, sunRot, asteroidScale,
                                                        glm::vec4(0.6f, 0.5f, 0.4f, 1.0f));
        Asteroid->InitSphere(asteroidSize * 8.0f, 15, 15);
        Asteroid->SetOrbitSpeed(1.5f + (rand() % 30) / 20.0f);
        Asteroid->SetOrbitDistance(distance);
        Asteroid->SetOrbitCenter(sunPos);
        Asteroid->SetParent(Uranus);
        MainGame->RegisterComponent("Asteroid" + std::to_string(i), Asteroid, InstanceSphereVertex, InstanceMaterial);
    }

    auto RandomFloat = [](float minValue, float maxValue)
    {
        float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return minValue + (maxValue - minValue) * t;
    };

    auto RandomStarColor = [&RandomFloat]()
    {
        float colorType = RandomFloat(0.0f, 1.0f);
        if (colorType < 0.55f)
        {
            return glm::vec4(RandomFloat(0.88f, 1.0f)*StartsItensity, RandomFloat(0.88f, 1.0f)*StartsItensity, RandomFloat(0.9f, 1.0f), 1.6f)*StartsItensity;
        }
        if (colorType < 0.80f)
        {
            return glm::vec4(RandomFloat(0.9f, 1.0f)*StartsItensity, RandomFloat(0.82f, 0.94f)*StartsItensity, RandomFloat(0.72f, 0.86f)*StartsItensity, 1.7f);
        }
        return glm::vec4(RandomFloat(0.72f, 0.86f)*StartsItensity, RandomFloat(0.82f, 0.94f)*StartsItensity, RandomFloat(0.94f, 1.0f)*StartsItensity, 1.7f);
    };

    float firstTheta = RandomFloat(0.0f, DirectX::XM_2PI);
    float firstCosPhi = RandomFloat(-1.0f, 1.0f);
    float firstSinPhi = sqrtf(1.0f - firstCosPhi * firstCosPhi);
    float firstDistance = RandomFloat(2600.0f, 3800.0f);
    glm::vec3 firstStarPos
    {
        firstDistance * firstSinPhi * cosf(firstTheta),
        firstDistance * firstCosPhi,
        firstDistance * firstSinPhi * sinf(firstTheta)
    };

    glm::vec3 firstStarScale{RandomFloat(0.5f, 1.8f), RandomFloat(0.5f, 1.8f), RandomFloat(0.5f, 1.8f)};
    SphereComponent* StarFieldAnchor = new SphereComponent(firstStarPos, sunRot, firstStarScale, RandomStarColor());
    StarFieldAnchor->InitSphere(1.1f, 6, 6);
    MainGame->RegisterComponent("StarFieldAnchor", StarFieldAnchor, InstanceSphereVertex, InstanceMaterial);

    for (int i = 1; i < StarCount; ++i)
    {
        float theta = RandomFloat(0.0f, DirectX::XM_2PI);
        float cosPhi = RandomFloat(-1.0f, 1.0f);
        float sinPhi = sqrtf(1.0f - cosPhi * cosPhi);
        float distance = RandomFloat(2600.0f, 3800.0f);
        glm::vec3 starPos
        {
            distance * sinPhi * cosf(theta),
            distance * cosPhi,
            distance * sinPhi * sinf(theta)
        };
        float minSize = 1.3f;
        float maxSize = 5.6f;
        float starSize = RandomFloat(minSize, maxSize);
        glm::vec3 starScale{starSize, starSize, starSize};
        SphereComponent* NewStar = StarFieldAnchor->CreateSphereInstance(starPos, sunRot, starScale, RandomStarColor());
        NewStar->SetOrbitSpeed(0.0f);
    }

    struct ConstellationEdge
    {
        int from;
        int to;
    };

    auto BuildConstellation = [&](const glm::vec3& skyDirection,
                                  float skyDistance,
                                  const std::vector<glm::vec2>& nodes,
                                  const std::vector<ConstellationEdge>& edges,
                                  const glm::vec4& starColor,
                                  const glm::vec4& lineColor)
    {
        glm::vec3 forward = glm::normalize(skyDirection);
        glm::vec3 upRef = (fabs(glm::dot(forward, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.95f)
                              ? glm::vec3(1.0f, 0.0f, 0.0f)
                              : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(upRef, forward));
        glm::vec3 up = glm::normalize(glm::cross(forward, right));
        constexpr float planeScale = 230.0f;

        std::vector<glm::vec3> worldNodes;
        worldNodes.reserve(nodes.size());

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            glm::vec3 worldPos = forward * skyDistance +
                right * (nodes[i].x * planeScale) +
                up * (nodes[i].y * planeScale);
            worldNodes.push_back(worldPos);

            float nodeSize = RandomFloat(5.5f, 8.5f);
            SphereComponent* NodeStar = StarFieldAnchor->CreateSphereInstance(
                worldPos,
                sunRot,
                glm::vec3(nodeSize, nodeSize, nodeSize),
                starColor
            );
            NodeStar->SetOrbitSpeed(0.0f);
        }

        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
        {
            const ConstellationEdge edge = edges[edgeIndex];
            if (edge.from < 0 || edge.to < 0 ||
                static_cast<size_t>(edge.from) >= worldNodes.size() ||
                static_cast<size_t>(edge.to) >= worldNodes.size())
            {
                continue;
            }

            glm::vec3 a = worldNodes[edge.from];
            glm::vec3 b = worldNodes[edge.to];
            float edgeLength = glm::length(b - a);
            int segmentCount = std::max(10, static_cast<int>(edgeLength / 34.0f));

            for (int segment = 1; segment < segmentCount; ++segment)
            {
                float t = static_cast<float>(segment) / static_cast<float>(segmentCount);
                glm::vec3 point = a + (b - a) * t;
                float pointSize = RandomFloat(1.4f, 2.2f);
                SphereComponent* LinePoint = StarFieldAnchor->CreateSphereInstance(
                    point,
                    sunRot,
                    glm::vec3(pointSize, pointSize, pointSize),
                    lineColor
                );
                LinePoint->SetOrbitSpeed(0.0f);
            }
        }
    };

    BuildConstellation(
        glm::vec3(-0.45f, 0.40f, 0.80f),
        4800.0f,
        {
            {-1.60f, 1.25f}, {-0.60f, 1.55f}, {0.50f, 1.30f}, {1.60f, 0.95f},
            {-0.75f, 0.35f}, {0.00f, 0.25f}, {0.75f, 0.15f}, {0.25f, -1.10f}
        },
        {
            {0, 1}, {1, 2}, {2, 3},
            {1, 4}, {4, 5}, {5, 6}, {6, 3},
            {5, 7}
        },
        glm::vec4(0.96f, 0.94f, 1.0f, 1.95f),
        glm::vec4(0.72f, 0.80f, 1.0f, 1.80f)
    );

    BuildConstellation(
        glm::vec3(0.72f, 0.25f, 0.64f),
        5000.0f,
        {
            {-2.10f, 1.10f}, {-1.00f, 0.30f}, {0.00f, 1.05f}, {1.05f, 0.20f}, {2.10f, 0.95f}
        },
        {
            {0, 1}, {1, 2}, {2, 3}, {3, 4}
        },
        glm::vec4(1.0f, 0.95f, 0.88f, 1.92f),
        glm::vec4(1.0f, 0.86f, 0.70f, 1.78f)
    );

    BuildConstellation(
        glm::vec3(0.10f, -0.10f, 0.99f),
        4700.0f,
        {
            {-1.20f, -0.20f}, {0.90f, -0.60f}, {0.10f, 1.10f}, {-0.40f, 0.25f}
        },
        {
            {0, 1}, {1, 2}, {2, 0}, {0, 3}, {3, 2}
        },
        glm::vec4(0.86f, 0.93f, 1.0f, 1.90f),
        glm::vec4(0.68f, 0.84f, 1.0f, 1.75f)
    );


    MainGame->StartGame();
}

void ForthLabStart(std::string Path)
{
    KatamariGame* MainGame = new KatamariGame();
    MainGame->Initialize();
    MainGame->SetDirectionalLight(SunDirectional,SunLightColor,SunItensity);
    MainGame->SetShadowSettings(ShadowCasting, ShadowDist);
    MainGame->SetGameType(CurrentGameType);
    MainGame->SetRenderingType(RenderingType);
    MainGame->CreateCubeMapFromFiles("MySky", {
                                         Path + "/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg",
                                         Path +"/cosy.jpg"
                                     });
    MainGame->GetPlayer()->SetCameraMode(CameraMode::Orbital);

    SkyboxComponent* Skybox = new SkyboxComponent(glm::vec3(4000.0f), glm::vec4(1.0f));
    Skybox->InitCube();
    Skybox->SetCubeMap(MainGame->GetCubeMap("MySky"));
    Skybox->SetReflectionSettings(SkyboxPower,100.f);
    MainGame->RegisterComponent("Skybox", Skybox, SkyboxMaterial, SkyboxVertex);

    PointLightComponent* MainWarmLight = new PointLightComponent(
        glm::vec3(0.0f, 100.0f, 0.0f),
        glm::vec3(1.0f, 0.85f, 0.6f),
        0.0f,
        5000.0f
    );
    
    PointLightComponent* BlueFillLight = new PointLightComponent(
        glm::vec3(-450.0f, 80.0f, 320.0f),
        glm::vec3(0.2f, 0.45f, 1.0f),
        2.2f,
        4000.0f
    );


    PointLightComponent* RedRimLight = new PointLightComponent(
        glm::vec3(520.0f, 60.0f, -420.0f),
        glm::vec3(1.0f, 0.25f, 0.2f),
        2.2f,
        4000.0f
    );
    MainGame->RegisterComponent("PointLight_RedRim", RedRimLight);
    MainGame->RegisterComponent("PointLight_BlueFill", BlueFillLight);
    MainGame->RegisterComponent("PointLight_MainWarm", MainWarmLight);

    glm::vec3 ComponentPosition{0.0f, -0.5f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotationBall{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{800.2f, 1.0f, 800.f};
    CubeComponent* Cube = new
        CubeComponent(ComponentPosition, ComponentRotation, ComponentScale, White);
    Cube->InitCube();
    MainGame->RegisterComponent("1", Cube);

    float BallRadius = 5.f;
    glm::vec3 BallScale{1.0f, 1.0f, 1.0f};
    glm::vec3 BallPosition{0.0f, BallRadius, 0.0f};
    KatamatiSphereComponent* Ball = new KatamatiSphereComponent(BallPosition, ComponentRotationBall, BallScale,
                                                                glm::vec4(1.0f, 0.9f, 0.3f, 1.0f));
    Ball->InitSphere(BallRadius, 100, 100);
    Ball->SetCollision(true);
    Ball->SetGame(MainGame);
    Ball->SetTexture(Path +"/1_Lab/Source/Models/2.jpg");
    Ball->SetCubeMap(MainGame->GetCubeMap("MySky"));
    Ball->SetReflectionSettings(0.72f, 5.5f);
    MainGame->RegisterComponent("Ball", Ball, ReflectiveSphereMaterial, ReflectiveSphereVertex);

    glm::vec3 SunAtmospherePosition{0.0f, 0.f, 0.0f};
    SphereComponent* SunAtmosphere = new SphereComponent(SunAtmospherePosition, ComponentRotation, BallScale,
                                                         glm::vec4(1.0f, 0.5f, 0.1f, 0.3f));
    SunAtmosphere->InitSphere(BallRadius + 2.f, 20, 20);
    SunAtmosphere->SetParent(Ball);
    MainGame->RegisterComponent("SunAtmosphere", SunAtmosphere, {}, SunMaterial);

    FBXComponent* TestCube = new FBXComponent(
        glm::vec3(10.0f, 2.0f, 10.0f), // позиция (рядом с шаром)
        glm::vec3(0.0f, 45.0f, 0.0f), // поворот на 45 градусов
        glm::vec3(2.0f, 2.0f, 2.0f) // масштаб в 2 раза больше
    );
    TestCube->SetGame(MainGame);
    TestCube->LoadModel(Path + "/1_Lab/Source/Models/test_cube.obj");
    TestCube->SetCubeMap(MainGame->GetCubeMap("MySky"));
    TestCube->SetReflectionSettings(0.45f, 4.0f);
    MainGame->RegisterComponent("TestCube", TestCube, ReflectiveFBXMaterial, ReflectiveFBXVertex);

    // Простой спавн объектов по кругу
    for (int indexModel = 1; indexModel < 5; indexModel++)
    {
        for (int i = 0; i < 360; i += 30) // Каждые 30 градусов
        {
            float angleRad = i * 3.14159f / 180.0f;
            float radius = 30.0f * indexModel;
            float x = cos(angleRad) * radius;
            float z = sin(angleRad) * radius;

            FBXComponent* Object = new FBXComponent(
                glm::vec3(x, 2.0f, z),
                glm::vec3(0.0f, 0.f, 0.0f),
                glm::vec3(11.0f, 10.0f, 10.0f),
                glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
            );
            
            //Вывод GBuffer
            Object->SetGame(MainGame);
            Object->SetCollision(true);
            int ModelNum = rand() % 3 + 1;
            //E:\ComputerGraphic\1_Lab
            Object->LoadModel(Path +"/1_Lab/Source/Models/" + std::to_string(ModelNum) + ".obj");
            Object->LoadTexture(Path + "/1_Lab/Source/Models/" + std::to_string(ModelNum) + ".jpg");
            Object->SetCubeMap(MainGame->GetCubeMap("MySky"));
            Object->SetReflectionSettings(0.08f, 1.0f);
            MainGame->RegisterComponent("Object_" + std::to_string(i) + std::to_string(indexModel), Object,
                                        FBXMaterial, FBXMaterial);
        }
    }


    MainGame->GetPlayer()->SetOrbitTarget(BallPosition);
    MainGame->GetPlayer()->StopRotate(true, false);
    MainGame->GetPlayer()->SetRotate(0.f, 0.6f);
    MainGame->GetPlayer()->EnableWASD(false);
    MainGame->GetPlayer()->SetOrbitRadius(50.f);
    MainGame->GetPlayer()->SetMinMaximalOrbitRadius(25.f, 200.f);
    MainGame->StartGame();
}
