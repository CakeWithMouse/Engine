#include "ExampleStartup.h"
#include <Engine/Base/Config/BaseResources.h>
#include <Engine/Base/Config/BaseGameConfig.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include "KatamariGame.h"
#include "KatamatiSphereComponent.h"
#include <Engine/Components/SpecificComponents/FBXComponent.h>
#include <Engine/Components/SpecificComponents/SphereComponent.h>
#include <Engine/Components/Light/CubeMap.h>
#include <Engine/Components/Light/PointLightComponent.h>

int RunScene(bool smoke, const std::filesystem::path& content)
{
    const BaseResources Resources;
    const BaseGameConfig Config;
    const std::string Path = content.generic_u8string();

    auto MainGame = std::make_unique<ExampleStartup::SmokeGame<KatamariGame>>(smoke);
    if (!MainGame->Initialize())
    {
        return 1;
    }
    MainGame->SetDirectionalLight(Config.SunDirectional,Config.SunLightColor,Config.SunItensity);
    MainGame->SetShadowSettings(Config.ShadowCasting, Config.ShadowDist);
    MainGame->SetRenderingType(Config.RenderingType);
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
    Skybox->SetReflectionSettings(Config.SkyboxPower,100.f);
    ExampleStartup::Register(*MainGame, "Skybox", Skybox, Resources.SkyboxMaterial, Resources.SkyboxVertex);

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
    ExampleStartup::Register(*MainGame, "PointLight_RedRim", RedRimLight);
    ExampleStartup::Register(*MainGame, "PointLight_BlueFill", BlueFillLight);
    ExampleStartup::Register(*MainGame, "PointLight_MainWarm", MainWarmLight);

    glm::vec3 ComponentPosition{0.0f, -0.5f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotationBall{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{800.2f, 1.0f, 800.f};
    CubeComponent* Cube = new
        CubeComponent(ComponentPosition, ComponentRotation, ComponentScale, Resources.White);
    Cube->InitCube();
    ExampleStartup::Register(*MainGame, "1", Cube);

    float BallRadius = 5.f;
    glm::vec3 BallScale{1.0f, 1.0f, 1.0f};
    glm::vec3 BallPosition{0.0f, BallRadius, 0.0f};
    KatamatiSphereComponent* Ball = new KatamatiSphereComponent(BallPosition, ComponentRotationBall, BallScale,
                                                                glm::vec4(1.0f, 0.9f, 0.3f, 1.0f));
    Ball->InitSphere(BallRadius, 100, 100);
    Ball->SetCollision(true);
    Ball->SetGame(MainGame.get());
    Ball->SetTexture(Path +"/Models/2.jpg");
    Ball->SetCubeMap(MainGame->GetCubeMap("MySky"));
    Ball->SetReflectionSettings(0.72f, 5.5f);
    ExampleStartup::Register(*MainGame, "Ball", Ball, Resources.ReflectiveSphereMaterial, Resources.ReflectiveSphereVertex);

    glm::vec3 SunAtmospherePosition{0.0f, 0.f, 0.0f};
    SphereComponent* SunAtmosphere = new SphereComponent(SunAtmospherePosition, ComponentRotation, BallScale,
                                                         glm::vec4(1.0f, 0.5f, 0.1f, 0.3f));
    SunAtmosphere->InitSphere(BallRadius + 2.f, 20, 20);
    SunAtmosphere->SetParent(Ball);
    ExampleStartup::Register(*MainGame, "SunAtmosphere", SunAtmosphere, {}, Resources.SunMaterial);

    FBXComponent* TestCube = new FBXComponent(
        glm::vec3(10.0f, 2.0f, 10.0f), // позиция (рядом с шаром)
        glm::vec3(0.0f, 45.0f, 0.0f), // поворот на 45 градусов
        glm::vec3(2.0f, 2.0f, 2.0f) // масштаб в 2 раза больше
    );
    TestCube->SetGame(MainGame.get());
    if (!TestCube->LoadModel(Path + "/Models/test_cube.obj")) { delete TestCube; throw std::runtime_error("Cannot load test_cube.obj"); }
    TestCube->SetCubeMap(MainGame->GetCubeMap("MySky"));
    TestCube->SetReflectionSettings(0.45f, 4.0f);
    ExampleStartup::Register(*MainGame, "TestCube", TestCube, Resources.ReflectiveFBXMaterial, Resources.ReflectiveFBXVertex);

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
            Object->SetGame(MainGame.get());
            Object->SetCollision(true);
            int ModelNum = rand() % 3 + 1;
            if (!Object->LoadModel(Path +"/Models/" + std::to_string(ModelNum) + ".obj")) { delete Object; throw std::runtime_error("Cannot load Katamari model"); }
            Object->LoadTexture(Path + "/Models/" + std::to_string(ModelNum) + ".jpg");
            Object->SetCubeMap(MainGame->GetCubeMap("MySky"));
            Object->SetReflectionSettings(0.08f, 1.0f);
            ExampleStartup::Register(*MainGame, "Object_" + std::to_string(i) + std::to_string(indexModel), Object,
                                        Resources.FBXMaterial, Resources.FBXMaterial);
        }
    }


    MainGame->GetPlayer()->SetOrbitTarget(BallPosition);
    MainGame->GetPlayer()->StopRotate(true, false);
    MainGame->GetPlayer()->SetRotate(0.f, 0.6f);
    MainGame->GetPlayer()->EnableWASD(false);
    MainGame->GetPlayer()->SetOrbitRadius(50.f);
    MainGame->GetPlayer()->SetMinMaximalOrbitRadius(25.f, 200.f);
    return MainGame->StartGame();

}

int main(int argc, char** argv)
{
    bool smoke = false;
    try
    {
        smoke = ExampleStartup::ParseSmoke(argc, argv);
        const auto content = ExampleStartup::PrepareContent();
        const int result = RunScene(smoke, content);
        if (smoke && result == 0) std::cout << "Katamari: 30 frames, normal shutdown\n";
        if (result != 0) throw std::runtime_error("Game initialization or execution failed");
        return result;
    }
    catch (const std::exception& error) { return ExampleStartup::ReportError(error, smoke); }
}
