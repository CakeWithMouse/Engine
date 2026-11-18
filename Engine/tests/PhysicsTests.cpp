#include <Engine/Physics/PhysicsWorld.h>
#include <cmath>
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
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

    // Equal impulses must produce velocities derived from the requested density,
    // not Jolt's default density. Test every supported primitive shape.
    for (const auto shape : {PhysicsShapeType::Box, PhysicsShapeType::Sphere, PhysicsShapeType::Capsule})
    {
        PhysicsBodyDesc lightDesc;
        lightDesc.shape = shape;
        lightDesc.position = glm::vec3(30.0f, 20.0f, 0.0f);
        lightDesc.density = 10.0f;
        auto heavyDesc = lightDesc;
        heavyDesc.position.x = 35.0f;
        heavyDesc.density = 100.0f;
        const auto light = world.CreateBody(lightDesc), heavy = world.CreateBody(heavyDesc);
        Require(light && heavy, "density test bodies were not created");
        const float pi = std::acos(-1.0f);
        const float sphereVolume = 4.0f / 3.0f * pi * .5f * .5f * .5f;
        const float volume = shape == PhysicsShapeType::Box ? 1.0f :
            shape == PhysicsShapeType::Sphere ? sphereVolume : sphereVolume + pi * .5f * .5f;
        world.AddImpulse(light, glm::vec3(100.0f, 0.0f, 0.0f));
        world.AddImpulse(heavy, glm::vec3(100.0f, 0.0f, 0.0f));
        const float lightSpeed = world.GetLinearVelocity(light).x;
        const float heavySpeed = world.GetLinearVelocity(heavy).x;
        std::cout << "Physics density shape=" << int(shape) << " light=" << lightSpeed
                  << " heavy=" << heavySpeed << " volume=" << volume << '\n';
        Require(Near(lightSpeed, 100.0f / (volume * lightDesc.density)),
                "dynamic body mass must use requested density");
        Require(Near(heavySpeed, 100.0f / (volume * heavyDesc.density)),
                "higher density must reduce equal-impulse velocity");
        world.DestroyBody(light);
        world.DestroyBody(heavy);
    }

    for (const float density : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()})
    {
        PhysicsBodyDesc invalid;
        invalid.density = density;
        Require(world.CreateBody(invalid) == 0, "invalid density must not create a body");
    }

    world.Shutdown();
    Require(!world.IsInitialized(), "world still initialized after shutdown");
}

// Complex collision: exact static triangles, convex moving/kinematic hulls.
void PhysicsMeshShapeTests()
{
    PhysicsSettings settings;
    PhysicsWorld world;
    std::string error;
    if (!world.Initialize(settings, error)) throw TestSkipped(error);

    // Unit cube as render geometry: 8 vertices, 12 triangles.
    const std::vector<glm::vec3> cubeVertices = {
        {-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f},
        {-.5f, -.5f,  .5f}, {.5f, -.5f,  .5f}, {.5f, .5f,  .5f}, {-.5f, .5f,  .5f}};
    const std::vector<uint32_t> cubeIndices = {
        0, 1, 5, 0, 5, 4,  3, 7, 6, 3, 6, 2,  4, 5, 6, 4, 6, 7,
        0, 3, 2, 0, 2, 1,  1, 2, 6, 1, 6, 5,  0, 4, 7, 0, 7, 3};

    // Static mesh floor: a wide concave shape; a ray straight down hits its top face.
    const std::vector<glm::vec3> floorVertices = {
        {-5.f, -.5f, -5.f}, {5.f, -.5f, -5.f}, {5.f, .5f, -5.f}, {-5.f, .5f, -5.f},
        {-5.f, -.5f,  5.f}, {5.f, -.5f,  5.f}, {5.f, .5f,  5.f}, {-5.f, .5f,  5.f}};
    PhysicsBodyDesc floorDesc;
    floorDesc.type = PhysicsBodyType::Static;
    floorDesc.shape = PhysicsShapeType::Mesh;
    floorDesc.meshVertices = &floorVertices;
    floorDesc.meshIndices = &cubeIndices;
    floorDesc.position = glm::vec3(0.0f, -0.5f, 0.0f); // top surface at y=0
    const uint32_t floor = world.CreateBody(floorDesc);
    Require(floor != 0, "static mesh body was not created");
    PhysicsRaycastHit hit;
    Require(world.Raycast(glm::vec3(1.0f, 5.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f), 100.0f, hit) &&
            hit.body == floor && Near(hit.position.y, 0.0f),
            "raycast missed the static mesh floor");

    // Kinematic mesh body is accepted as well.
    PhysicsBodyDesc kinematicDesc;
    kinematicDesc.type = PhysicsBodyType::Kinematic;
    kinematicDesc.shape = PhysicsShapeType::Mesh;
    kinematicDesc.meshVertices = &cubeVertices;
    kinematicDesc.meshIndices = &cubeIndices;
    kinematicDesc.position = glm::vec3(20.0f, 0.0f, 0.0f);
    const uint32_t kinematic = world.CreateBody(kinematicDesc);
    Require(kinematic != 0, "kinematic mesh body was not created");

    // Dynamic mesh body: Jolt wraps the render mesh in a convex hull, so the body
    // must fall and settle on the floor like a unit box (center rest height 0.5).
    PhysicsBodyDesc boxDesc;
    boxDesc.type = PhysicsBodyType::Dynamic;
    boxDesc.shape = PhysicsShapeType::Mesh;
    boxDesc.meshVertices = &cubeVertices;
    boxDesc.meshIndices = &cubeIndices;
    boxDesc.position = glm::vec3(0.0f, 3.0f, 0.0f);
    const uint32_t box = world.CreateBody(boxDesc);
    Require(box != 0, "dynamic mesh body was not created");
    for (int i = 0; i < 900; ++i) world.Update(settings.GetFixedTimeStep());
    Require(Near(world.GetPosition(box).y, 0.5f, 0.1f), "dynamic mesh body did not settle on the floor");
    Require(std::fabs(world.GetLinearVelocity(box).y) < 0.5f, "mesh body did not settle");

    // The hull volume is the unit cube's, so the requested density drives the mass:
    // an impulse of 100 N*s on a 10 kg body (density 10 * volume 1) yields 10 m/s.
    PhysicsBodyDesc lightDesc;
    lightDesc.shape = PhysicsShapeType::Mesh;
    lightDesc.meshVertices = &cubeVertices;
    lightDesc.meshIndices = &cubeIndices;
    lightDesc.position = glm::vec3(0.0f, 30.0f, 0.0f);
    lightDesc.density = 10.0f;
    const uint32_t light = world.CreateBody(lightDesc);
    Require(light != 0, "density mesh body was not created");
    world.AddImpulse(light, glm::vec3(100.0f, 0.0f, 0.0f));
    Require(Near(world.GetLinearVelocity(light).x, 10.0f), "mesh body mass must use requested density");

    // Degenerate input must not create a body: missing geometry and empty index list.
    PhysicsBodyDesc invalidDesc;
    invalidDesc.shape = PhysicsShapeType::Mesh;
    Require(world.CreateBody(invalidDesc) == 0, "mesh body without geometry must not be created");
    const std::vector<uint32_t> noTriangles;
    invalidDesc.meshVertices = &cubeVertices;
    invalidDesc.meshIndices = &noTriangles;
    Require(world.CreateBody(invalidDesc) == 0, "mesh body without triangles must not be created");

    world.Shutdown();
}

void PhysicsMeshComponentTests()
{
    class CpuGame : public Game { public: using Game::TickPhysicsWorld; };
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        CpuGame game;
        std::unique_ptr<GameComponent> parent = std::make_unique<GameComponent>();
        auto* parentPtr = parent.get();
        Require(game.GetScene().AddComponent("parent", parent).result == SceneResult::Ok, "parent registration");
        std::unique_ptr<GameComponent> owner = std::make_unique<GameComponent>();
        auto* body = owner.get();
        MeshLodData mesh;
        for (const auto& p : {glm::vec3(1,0,0), glm::vec3(2,0,0), glm::vec3(1,0,1)}) {
            MeshVertex vertex; vertex.Position = {p.x,p.y,p.z,1}; mesh.Vertices.push_back(vertex);
        }
        mesh.Indices = {0,2,1};
        body->SetPrimitive(std::move(mesh));
        body->SetPosition({3,1,2});
        body->SetScale({scenario ? -2.f : 2.f,1,1});
        body->SetPhysicsBodyType(PhysicsBodyType::Static);
        body->SetPhysicsShapeType(PhysicsShapeType::Mesh);
        body->SetPhysicsEnabled(true);
        Require(game.GetScene().AddComponent("mesh", owner).result == SceneResult::Ok, "mesh registration");
        auto expected = glm::translate(glm::mat4(1), glm::vec3(3,1,2));
        if (scenario == 2) {
            parentPtr->SetScale({2,1,3}); parentPtr->SetRotation({0,40,0}); body->SetRotation({0,25,0});
            Require(body->SetParent(parentPtr), "mesh parent assignment");
            expected = glm::rotate(glm::mat4(1),glm::radians(40.f),glm::vec3(0,1,0)) *
                glm::scale(glm::mat4(1),glm::vec3(2,1,3)) * expected *
                glm::rotate(glm::mat4(1),glm::radians(25.f),glm::vec3(0,1,0));
        }
        expected = expected * glm::scale(glm::mat4(1),glm::vec3(scenario ? -2.f : 2.f,1,1));
        const auto point = glm::vec3(expected * glm::vec4(4.f/3.f,0,1.f/3.f,1));
        const Trace::Ray ray{point + glm::vec3(0,2,0), {0,-1,0}, 5};
        Require(bool(game.GetScene().Raycast(ray)), "reference ray must hit the rendered triangle");
        game.InitializeGameplay(); game.TickPhysicsWorld(0);
        if (!game.GetPhysicsWorld()) throw TestSkipped("Jolt not available");
        body->Tick(0);
        Require(body->HasPhysicsBody(), "component mesh body missing");
        PhysicsRaycastHit hit;
        const bool found = game.GetPhysicsWorld()->Raycast(ray.origin,ray.direction,ray.maxDistance,hit);
        std::cout << "Mesh transform scenario=" << scenario << " physicsHit=" << found << '\n';
        Require(found && glm::length(hit.position-point)<.001f,
                "mesh collision must match render geometry after signed/parent transforms");
    }
}

void PhysicsMeshIndexedHullTests()
{
    PhysicsWorld world; PhysicsSettings settings; std::string error;
    if (!world.Initialize(settings,error)) throw TestSkipped(error);
    std::vector<glm::vec3> vertices = {{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f},{100,100,100}};
    std::vector<uint32_t> indices = {0,1,5,0,5,4,3,7,6,3,6,2,4,5,6,4,6,7,
        0,3,2,0,2,1,1,2,6,1,6,5,0,4,7,0,7,3};
    PhysicsBodyDesc desc; desc.shape=PhysicsShapeType::Mesh; desc.meshVertices=&vertices;
    desc.meshIndices=&indices; desc.density=10; desc.position={0,30,0};
    const auto body=world.CreateBody(desc);
    Require(body!=0,"indexed hull missing");
    world.AddImpulse(body,{100,0,0});
    std::cout << "Indexed hull impulse speed=" << world.GetLinearVelocity(body).x << '\n';
    Require(Near(world.GetLinearVelocity(body).x,10),"unused render vertices must not inflate moving mesh hull/mass");
    indices={100,101,102};
    Require(world.CreateBody(desc)==0,"moving mesh with no valid indexed triangles must be rejected");
    indices={0,1,2};
    for (auto& vertex : vertices) vertex.x=std::numeric_limits<float>::quiet_NaN();
    Require(world.CreateBody(desc)==0,"non-finite mesh triangles must be rejected safely");
}
