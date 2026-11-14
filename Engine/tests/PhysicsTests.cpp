#include <Engine/Physics/PhysicsWorld.h>
#include <cmath>
#include <stdexcept>

#include "TestSkipped.h"

namespace
{
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
bool Near(float a, float b, float epsilon = 1e-3f)
{
    return std::fabs(a - b) <= epsilon;
}
}

// Jolt-driven rigid body lifecycle: gravity, fixed stepping, impulses, raycasts, body removal.
void PhysicsWorldTests()
{
    PhysicsSettings settings;
    PhysicsWorld world;
    std::string error;
    if (!world.Initialize(settings, error)) throw TestSkipped(error);

    // Static floor at y=0 (top surface at y=0.5) and a dynamic unit box above it.
    PhysicsBodyDesc floorDesc;
    floorDesc.type = PhysicsBodyType::Static;
    floorDesc.shape = PhysicsShapeType::Box;
    floorDesc.position = glm::vec3(0.0f, 0.0f, 0.0f);
    floorDesc.halfExtents = glm::vec3(10.0f, 0.5f, 10.0f);
    const uint32_t floor = world.CreateBody(floorDesc);
    Require(floor != 0, "floor body was not created");

    PhysicsBodyDesc boxDesc;
    boxDesc.type = PhysicsBodyType::Dynamic;
    boxDesc.shape = PhysicsShapeType::Box;
    boxDesc.position = glm::vec3(0.0f, 5.0f, 0.0f);
    boxDesc.halfExtents = glm::vec3(0.5f, 0.5f, 0.5f);
    const uint32_t box = world.CreateBody(boxDesc);
    Require(box != 0 && box != floor, "dynamic body was not created");
    Require(world.GetBodyCount() == 2, "unexpected body count");
    Require(Near(world.GetLinearVelocity(box).y, 0.0f), "initial velocity is not zero");

    // The box must fall under gravity and come to rest on the floor.
    bool bouncedOrLanded = false;
    for (int i = 0; i < 600; ++i)
    {
        world.Update(settings.GetFixedTimeStep());
        if (world.GetPosition(box).y < 4.0f) bouncedOrLanded = true;
    }
    Require(bouncedOrLanded, "dynamic body did not fall under gravity");
    const float restY = world.GetPosition(box).y;
    Require(Near(restY, 1.0f, 0.1f), "dynamic body did not come to rest on the floor");
    Require(std::fabs(world.GetLinearVelocity(box).y) < 0.5f, "body did not settle");

    // An upward impulse launches the box again (unit cube at default density weighs 1000 kg).
    world.AddImpulse(box, glm::vec3(0.0f, 5000.0f, 0.0f));
    Require(world.GetLinearVelocity(box).y > 1.0f, "impulse did not change velocity");
    for (int i = 0; i < 600; ++i) world.Update(settings.GetFixedTimeStep());
    Require(Near(world.GetPosition(box).y, restY, 0.25f), "body did not land back on the floor");

    // Ray straight down hits the box first, then the floor after the box is removed.
    PhysicsRaycastHit hit;
    Require(world.Raycast(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 100.0f, hit),
            "raycast missed the dynamic box");
    Require(hit.body == box && hit.normal.y > 0.9f && hit.distance > 8.0f, "raycast hit wrong body");
    world.DestroyBody(box);
    Require(!world.IsValid(box), "destroyed body handle is still valid");
    Require(world.GetBodyCount() == 1, "body count did not drop after destroy");
    Require(world.Raycast(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 100.0f, hit) && hit.body == floor,
            "raycast missed the floor");

    // Settings round-trip: gravity and step values reach the world unchanged.
    settings.SetGravity(glm::vec3(0.0f, -3.0f, 0.0f));
    settings.SetFixedTimeStep(1.0f / 120.0f);
    world.ApplySettings(settings);
    Require(world.GetSettings().GetGravity().y == -3.0f && world.GetSettings().GetFixedTimeStep() == 1.0f / 120.0f,
            "settings were not applied");

    world.Shutdown();
    Require(!world.IsInitialized(), "world still initialized after shutdown");
}
