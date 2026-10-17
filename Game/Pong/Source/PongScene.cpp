#include "ExampleStartup.h"
#include <Engine/Base/Config/BaseResources.h>
#include <Engine/Base/Config/BaseGameConfig.h>
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include "PongGame.h"
#include "Ball.h"
#include "Rocket.h"
#include "Wall.h"

void PongGame::BuildScene()
{
    const BaseResources Resources;
    glm::vec3 ComponentPositionLeft{-4.5f, 0.0f, 0.0f};
    glm::vec3 ComponentRotation{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScale{0.2f, 2.5f, 1.f};
    RocketComponent* Cube = new
        RocketComponent(ComponentPositionLeft, ComponentRotation, ComponentScale, Resources.White);
    Cube->InitCube();
    Cube->SetRocketType(RocketType::Left);
    Cube->SetCollision(true);
    ExampleStartup::Register(*this, "5", Cube);

    glm::vec3 ComponentPositionRight{4.5f, 0.0f, 0.0f};
    RocketComponent* Cube2 = new RocketComponent(ComponentPositionRight, ComponentRotation, ComponentScale,
                                                 Resources.White);
    Cube2->InitCube();
    Cube2->SetCollision(true);
    Cube2->SetRocketType(RocketType::Right);
    ExampleStartup::Register(*this, "6", Cube2);

    glm::vec3 ComponentPositionCenter{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentRotationBall{0.0f, 0.0f, 0.0f};
    glm::vec3 ComponentScaleBall{0.3f, 0.3f, 1.f};
    BallComponent* Ball = new BallComponent(ComponentPositionCenter, ComponentRotationBall, ComponentScaleBall,
                                            Resources.Blue);
    Ball->InitCube();
    ExampleStartup::Register(*this, "PongBall", Ball);

    glm::vec3 ComponentScaleWall{0.08f, 15.f, 1.f};
    CubeComponent* WallCenter = new CubeComponent(ComponentPositionCenter, ComponentRotation,
                                                  ComponentScaleWall,
                                                  Resources.White);
    WallCenter->InitCube();
    ExampleStartup::Register(*this, "4", WallCenter);

    glm::vec3 ComponentPositionCenterUp{0.f, 4.f, 1.f};
    glm::vec3 ComponentScaleWall2{0.12f, 1.f, 1.f};
    CubeComponent* WallCenterUp = new CubeComponent(ComponentPositionCenterUp, ComponentRotation,
                                                    ComponentScaleWall2,
                                                    Resources.Green);
    WallCenterUp->InitCube();
    ExampleStartup::Register(*this, "7", WallCenterUp);

    glm::vec3 ComponentPositionCenterDown{0.f, -4.f, 1.f};
    CubeComponent* WallCenterDown = new CubeComponent(ComponentPositionCenterDown, ComponentRotation,
                                                      ComponentScaleWall2,
                                                      Resources.Green);
    WallCenterDown->InitCube();
    ExampleStartup::Register(*this, "8", WallCenterDown);

    glm::vec3 ComponentPositionCenterWall{0.f, 0.f, 1.f};
    CubeComponent* WallCenterWall = new CubeComponent(ComponentPositionCenterWall, ComponentRotation,
                                                      ComponentScaleWall2,
                                                      Resources.Green);
    WallCenterWall->InitCube();
    ExampleStartup::Register(*this, "9", WallCenterWall);

    glm::vec3 ComponentScaleWallEnd{0.1f, 15.f, 1.f};
    glm::vec3 ComponentPositionLefWallt{-5.2f, 0.0f, 0.0f};
    Wall* WallLeft = new Wall(ComponentPositionLefWallt, ComponentRotation, ComponentScaleWallEnd,
                              Resources.Red);
    WallLeft->InitCube();
    WallLeft->SetCollision(true);
    WallLeft->SetWallType(WallType::LeftWall);
    ExampleStartup::Register(*this, "1", WallLeft);
    glm::vec3 ComponentPositionRightWall{5.2f, 0.0f, 0.0f};
    Wall* WallRight = new Wall(ComponentPositionRightWall, ComponentRotation, ComponentScaleWallEnd,
                               Resources.Red);
    WallRight->InitCube();
    WallRight->SetCollision(true);
    WallRight->SetWallType(WallType::RightWall);
    ExampleStartup::Register(*this, "2", WallRight);

}
