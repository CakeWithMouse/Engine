#include "../Source/Components/ComponentInternal.h"
#define NOMINMAX
#include <Engine/Scene/Scene.h>
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Runtime/SceneEditSession.h>
#include <Engine/Components/Light/PointLightComponent.h>
#include <Engine/Components/SpecificComponents/UIComponent.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#define CHECK(v)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(v))                                                                                                      \
            throw std::runtime_error("Trace: " #v);                                                                    \
    } while (false)
namespace
{
bool Near(float a, float b)
{
    return std::abs(a - b) < .002f;
}
bool Near(glm::vec3 a, glm::vec3 b)
{
    return glm::length(a - b) < .002f;
}
MeshLodData Triangle()
{
    MeshLodData m;
    for (auto p : {glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(0, 1, 0)})
    {
        MeshVertex v{};
        v.Position = {p.x, p.y, p.z, 1};
        m.Vertices.push_back(v);
    }
    m.Indices = {0, 1, 2};
    return m;
}
uint64_t Add(Scene &s, const char *name, float z = 0)
{
    auto mesh = std::make_unique<MeshComponent>();
    mesh->SetMesh(Triangle());
    mesh->SetPosition({0, 0, z});
    std::unique_ptr<GameComponent> p = std::move(mesh);
    return s.AddComponent(name, p).id;
}
DirectX::XMFLOAT4X4 Stored(DirectX::FXMMATRIX m)
{
    DirectX::XMFLOAT4X4 out;
    DirectX::XMStoreFloat4x4(&out, DirectX::XMMatrixTranspose(m));
    return out;
}
Game *playing = nullptr;
std::unique_ptr<Game> MakeGame()
{
    auto g = std::make_unique<Game>();
    playing = g.get();
    return g;
}
} // namespace
void TraceMathTests()
{
    using namespace Trace;
    Ray ray{{0, 0, 5}, {0, 0, -7}, 10};
    auto p = IntersectPlane(ray, {0, 0, 0}, {0, 0, 3});
    CHECK(p && Near(p.hit->distance, 5) && Near(p.hit->normal, {0, 0, 1}));
    CHECK(!IntersectPlane(ray, {0, 0, 0}, {1, 0, 0}));
    CHECK(!IntersectPlane(ray, {0, 0, 0}, {0, 0, -1}, Backfaces::Cull));
    CHECK(IntersectPlane(ray, {0, 0, 0}, {0, 0, 0}).status == Status::InvalidQuery);
    auto b = IntersectAabb(ray, {-1, -1, -1}, {1, 1, 1});
    CHECK(b && Near(b.hit->distance, 4) && Near(b.hit->normal, {0, 0, 1}));
    b = IntersectAabb({{0, 0, 0}, {1, 0, 0}, 5}, {-1, -1, -1}, {1, 1, 1});
    CHECK(b && Near(b.hit->distance, 1) && Near(b.hit->normal, {1, 0, 0}));
    CHECK(!IntersectAabb({{2, 0, 5}, {0, 0, -1}, 10}, {-1, -1, -1}, {1, 1, 1}));
    CHECK(IntersectAabb(ray, {1, 1, 1}, {-1, -1, -1}).status == Status::InvalidQuery);
    auto t = IntersectTriangle(ray, {-1, -1, 0}, {1, -1, 0}, {0, 1, 0});
    CHECK(t && Near(t.hit->distance, 5));
    CHECK(!IntersectTriangle(ray, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}));
    CHECK(!IntersectTriangle(ray, {-1, -1, 0}, {0, 1, 0}, {1, -1, 0}, Backfaces::Cull));
    ray.maxDistance = 4.99f;
    CHECK(!IntersectTriangle(ray, {-1, -1, 0}, {1, -1, 0}, {0, 1, 0}));
    ray.maxDistance = 5;
    CHECK(IntersectTriangle(ray, {-1, -1, 0}, {1, -1, 0}, {0, 1, 0}));
    CHECK(!Normalize({{0, 0, 0}, {0, 0, 0}, 1}));
    CHECK(Normalize({{0, 0, 0}, {0, 0, 1e-30f}, 1}));
    CHECK(!Normalize({{NAN, 0, 0}, {0, 0, 1}, 1}));
    CHECK(!Normalize({{0, 0, 0}, {0, INFINITY, 1}, 1}));
    CHECK(!Normalize({{0, 0, 0}, {0, 0, 1}, -1}));
    CHECK(!Normalize({{0, 0, 0}, {0, 0, 1}, INFINITY}));
}
void TraceSceneTests()
{
    using namespace Trace;
    Scene s;
    auto farId = Add(s, "farId", -3), nearId = Add(s, "nearId");
    Ray ray{{0, 0, 5}, {0, 0, -2}, 100};
    auto r = s.Raycast(ray);
    CHECK(r && r.hit->componentId == nearId && Near(r.hit->distance, 5) && r.hit->triangleIndex == 0);
    CHECK(r.statistics.queries == 1 && r.statistics.bounds == 2 && r.statistics.triangles == 2 &&
          r.statistics.microseconds >= 0);
    Query q;
    q.ignore = {nearId};
    CHECK(s.Raycast(ray, q).hit->componentId == farId);
    q.ignore = {nearId, farId};
    CHECK(s.Raycast(ray, q).status == Status::Miss);
    q = {};
    s.FindComponent(nearId)->traceChannels = 2;
    q.channels = 1;
    CHECK(s.Raycast(ray, q).hit->componentId == farId);
    q.channels = 0;
    CHECK(!s.Raycast(ray, q));
    q = {};
    q.maxDistance = 4.99f;
    CHECK(!s.Raycast(ray, q));
    q.maxDistance = 5;
    CHECK(s.Raycast(ray, q));
    ray.maxDistance = 4;
    CHECK(!s.Raycast(ray));
    ray.maxDistance = 100;
    CHECK(s.Raycast({{0, 0, 0}, {0, 0, 0}, 100}).status == Status::InvalidRay);
    q = {};
    q.maxDistance = NAN;
    CHECK(s.Raycast(ray, q).status == Status::InvalidQuery);
    // Bounding rectangle contains this point, but the triangle does not.
    Ray corner{{.9f, .9f, 5}, {0, 0, -1}, 100};
    CHECK(!s.Raycast(corner));
    q = {};
    q.geometry = Geometry::Bounds;
    CHECK(s.Raycast(corner, q));
    auto *child = s.FindComponent(nearId);
    child->SetScale({-2, 3, 4});
    q = {};
    q.ignore = {farId};
    r = s.Raycast(ray, q);
    CHECK(r && Near(r.hit->normal, {0, 0, -1}) && Near(r.hit->distance, 5));
    q.backfaces = Backfaces::Cull;
    CHECK(!s.Raycast(ray, q));
    std::unique_ptr<GameComponent> parent = std::make_unique<GameComponent>();
    auto *ptr = parent.get();
    auto parentId = s.AddComponent("parent", parent).id;
    ptr->SetPosition({3, 0, -2});
    ptr->SetScale({2, 1, 3});
    ptr->SetRotation({0, 30, 0});
    CHECK(child->SetParent(ptr));
    const auto matrix = EngineInternal::ComponentAccess::WorldMatrix(*child);
    using namespace DirectX;
    auto world = [&](float x, float y) {
        XMFLOAT3 v;
        XMStoreFloat3(&v, XMVector3TransformCoord(XMVectorSet(x, y, 0, 1), XMLoadFloat4x4(&matrix)));
        return glm::vec3(v.x, v.y, v.z);
    };
    auto a = world(-1, -1), b = world(1, -1), c = world(0, 1);
    auto normal = glm::normalize(glm::cross(b - a, c - a));
    auto center = (a + b + c) / 3.f;
    q.backfaces = Backfaces::Include;
    r = s.Raycast({center + normal * 7.f, -normal * 5.f, 10}, q);
    CHECK(r && r.hit->componentId == nearId && Near(r.hit->distance, 7) && Near(r.hit->normal, normal) &&
          Near(r.hit->position, center));
    child->SetScale({0, 1, 1});
    CHECK(!s.Raycast({center + normal * 7.f, -normal, 10}, q));
    // Helper spheres are available only to editor selection.
    Scene helpers;
    std::unique_ptr<GameComponent> helper = std::make_unique<GameComponent>();
    helpers.AddComponent("helper", helper);
    CHECK(!helpers.Raycast(ray));
    q = {};
    q.geometry = Geometry::Bounds;
    CHECK(!helpers.Raycast(ray, q));
    q.geometry = Geometry::EditorSelection;
    CHECK(helpers.Raycast(ray, q));
    Scene instances;
    auto source = Add(instances, "copies");
    auto *src = instances.FindComponent(source);
    src->CreateInstance({4, 0, 0}, {0, 0, 0}, {1, 1, 1}, {1, 1, 1, 1});
    CHECK(!instances.Raycast(ray));
    r = instances.Raycast({{4, 0, 5}, {0, 0, -1}, 10});
    CHECK(r && r.hit->instanceIndex == 0 && r.hit->componentId == source);
    auto batch = std::make_unique<InstancedMeshComponent>();
    CHECK(batch->SetLod(0, Triangle()));
    MeshInstance instance;
    instance.Position = {8, 0, 0};
    batch->AddInstance(instance);
    std::unique_ptr<GameComponent> owned = std::move(batch);
    auto id = instances.AddComponent("batch", owned).id;
    r = instances.Raycast({{8, 0, 5}, {0, 0, -1}, 10});
    CHECK(r && r.hit->componentId == id && r.hit->instanceIndex == 0);
    {
        auto update = instances.BeginUpdate();
        CHECK(instances.RemoveComponent(id) == SceneResult::Deferred);
        CHECK(!instances.Raycast({{8, 0, 5}, {0, 0, -1}, 10}));
    }
    Scene ties;
    auto first = Add(ties, "first"), second = Add(ties, "second");
    CHECK(ties.Raycast(ray).hit->componentId == first);
    Scene animated;
    auto model = std::make_shared<SkeletalModel>();
    SkeletalMesh mesh;
    for (const auto &v : Triangle().Vertices)
    {
        SkinnedVertex skinned{};
        skinned.Position = {v.Position.x, v.Position.y, v.Position.z};
        mesh.Vertices.push_back(skinned);
    }
    mesh.Indices = {0, 1, 2};
    model->Meshes.push_back(mesh);
    model->TotalIndexCount = 3;
    model->Clips.emplace_back();
    auto skeletal = std::make_unique<SkeletalMeshComponent>();
    CHECK(skeletal->SetModel(model));
    std::unique_ptr<GameComponent> animatedObject = std::move(skeletal);
    animated.AddComponent("animated", animatedObject);
    r = animated.Raycast(ray);
    CHECK(!r && r.statistics.unsupported == 1);
    q = {};
    q.geometry = Geometry::Bounds;
    CHECK(animated.Raycast(ray, q));
    q.geometry = Geometry::EditorSelection;
    CHECK(animated.Raycast(ray, q));
    s.ResetTraceStatistics();
    CHECK(s.GetTraceStatistics().queries == 0);
    s.Raycast(ray);
    CHECK(s.GetTraceStatistics().queries == 1);
}
void TraceScreenTests()
{
    using namespace DirectX;
    using namespace Trace;
    const auto view = Stored(XMMatrixIdentity()),
               perspective = Stored(XMMatrixPerspectiveFovRH(XM_PIDIV2, 2, .1f, 100));
    auto ray = ScreenPointToRay({100, 50}, 200, 100, view, perspective);
    CHECK(ray && Near(ray.ray.origin, {0, 0, -.1f}) && Near(ray.ray.direction, {0, 0, -1}));
    auto tl = ScreenPointToRay({0, 0}, 200, 100, view, perspective);
    CHECK(tl && tl.ray.direction.x < 0 && tl.ray.direction.y > 0);
    CHECK(ScreenPointToRay({199.99f, 99.99f}, 200, 100, view, perspective));
    CHECK(ScreenPointToRay({200, 50}, 200, 100, view, perspective).status == Status::OutsideViewport);
    CHECK(ScreenPointToRay({-1, 50}, 200, 100, view, perspective).status == Status::OutsideViewport);
    auto pixel = ScreenPointToRay(ViewportPoint::Pixel(100, 50), 200, 100, view, perspective);
    CHECK(pixel && pixel.ray.direction.x > 0 && pixel.ray.direction.y < 0);
    auto fractional = ScreenPointToRay({100.25f, 50.25f}, 200, 100, view, perspective);
    CHECK(fractional && fractional.ray.direction.x > 0 && fractional.ray.direction.x < pixel.ray.direction.x);
    auto dpi = ViewportPoint::Logical(50.125f, 25.125f, 2);
    CHECK(dpi.x == 100.25f && dpi.y == 50.25f);
    CHECK(Near(ScreenPointToRay(dpi, 200, 100, view, perspective).ray.direction, fractional.ray.direction));
    CHECK(
        Near(ScreenPointToRay({200.5f, 100.5f}, 400, 200, view, perspective).ray.direction, fractional.ray.direction));
    CHECK(ScreenPointToRay({0, 0}, 0, 100, view, perspective).status == Status::InvalidViewport);
    CHECK(ScreenPointToRay({NAN, 0}, 200, 100, view, perspective).status == Status::InvalidQuery);
    XMFLOAT4X4 zero{};
    CHECK(ScreenPointToRay({1, 1}, 200, 100, zero, perspective).status == Status::InvalidMatrix);
    auto bad = view;
    bad._11 = NAN;
    CHECK(ScreenPointToRay({1, 1}, 200, 100, bad, perspective).status == Status::InvalidMatrix);
    // XY, XZ and YZ views. Directions are parallel, origins vary in each viewing plane.
    for (auto direction : {glm::vec3(0, 0, -1), glm::vec3(0, -1, 0), glm::vec3(-1, 0, 0)})
    {
        auto up = std::abs(direction.y) > .9f ? XMVectorSet(0, 0, -1, 0) : XMVectorSet(0, 1, 0, 0);
        auto v = Stored(XMMatrixLookToRH(XMVectorZero(), XMVectorSet(direction.x, direction.y, direction.z, 0), up));
        auto ortho = Stored(XMMatrixOrthographicRH(20, 10, .1f, 100));
        auto first = ScreenPointToRay({20, 20}, 200, 100, v, ortho),
             second = ScreenPointToRay({180, 80}, 200, 100, v, ortho);
        CHECK(first && second && Near(first.ray.direction, direction) &&
              Near(first.ray.direction, second.ray.direction));
        CHECK(glm::length(first.ray.origin - second.ray.origin) > 10);
        CHECK(Near(glm::dot(first.ray.origin - second.ray.origin, direction), 0));
        auto centerRay = ScreenPointToRay({100, 50}, 200, 100, v, perspective);
        CHECK(centerRay && Near(centerRay.ray.direction, direction));
    }
}
void TraceGameTests()
{
    using namespace Trace;
    Game empty;
    CHECK(empty.ScreenPointToRay({0, 0}).status == Status::NoCamera);
    empty.GetInput().SetContextEnabled(Input::Context::Game, true);
    CHECK(empty.RaycastCursor().status == Status::CursorUnavailable);
    empty.GetInput().SetPointerBlocked(true);
    CHECK(empty.RaycastCursor().status == Status::InputBlocked);
    empty.GetInput().SetPointerBlocked(false);
    empty.GetInput().SetCursorPosition({1, 1, 100, 100, true});
    empty.OnFocusLost();
    CHECK(!empty.GetInput().GetCursorPosition().valid);
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"Trace tests", WS_OVERLAPPEDWINDOW, 0, 0, 320, 200, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    CHECK(hwnd);
    struct Guard
    {
        HWND h;
        ~Guard()
        {
            DestroyWindow(h);
        }
    } guard{hwnd};
    const ComponentType types[] = {{"Test.Triangle", "Triangle",
                                    []() -> std::unique_ptr<GameComponent> {
                                        auto m = std::make_unique<MeshComponent>();
                                        m->SetMesh(Triangle());
                                        return m;
                                    }},
                                   {"Test.Light", "Light", []() -> std::unique_ptr<GameComponent> {
                                        return std::make_unique<PointLightComponent>(glm::vec3(0), glm::vec3(1));
                                    }}};
    SceneEditSession session({types, 2}, MakeGame);
    auto api = session.GetApi();
    auto cursor = session.GetViewportCursorApi();
    auto input = session.GetViewportInputApi();
    CHECK(cursor.version == 1 && cursor.set && cursor.context);
    CHECK(cursor.set(cursor.context, 1, 1, 320, 200, 1) == EngineSceneInvalidArgument);
    CHECK(api.attachViewport(api.context, hwnd, 320, 200) == EngineSceneOk);
    CHECK(api.frame(api.context, 0) == EngineSceneOk);
    uint64_t picked = 99;
    CHECK(api.pickObject(api.context, 0, 0, &picked) == EngineSceneOk && picked == 0);
    EngineSceneTransform identity{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
    uint64_t objectId = 0;
    for (const auto *type : {"Test.Triangle", "Test.Light"})
    {
        CHECK(api.scene.createObject(api.scene.context, type, "pick", &identity, &objectId) == EngineSceneOk);
        CHECK(api.pickObject(api.context, 0, 0, &picked) == EngineSceneOk && picked == objectId);
        CHECK(api.scene.removeObject(api.scene.context, objectId) == EngineSceneOk);
    }
    CHECK(input.focus(input.context, 1) == EngineSceneOk);
    CHECK(api.control(api.context, EnginePlay) == EngineSceneOk);
    CHECK(playing);
    CHECK(cursor.set(cursor.context, NAN, 1, 320, 200, 1) == EngineSceneInvalidArgument);
    CHECK(cursor.set(cursor.context, 10, 10, 100, 100, 1) == EngineSceneInvalidArgument);
    CHECK(api.frame(api.context, 0) == EngineSceneOk);
    CHECK(cursor.set(cursor.context, 160.25f, 100.25f, 320, 200, 1) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::Miss);
    auto ray = playing->ScreenPointToRay({160.25f, 100.25f});
    CHECK(ray);
    CHECK(cursor.set(cursor.context, -1, 10, 320, 200, 1) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::OutsideViewport);
    CHECK(cursor.set(cursor.context, 10, 10, 320, 200, 0) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::CursorUnavailable);
    CHECK(input.focus(input.context, 0) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::InputBlocked);
    CHECK(input.focus(input.context, 1) == EngineSceneOk);
    CHECK(cursor.set(cursor.context, 10, 10, 320, 200, 1) == EngineSceneOk);
    playing->GetInput().SetPointerBlocked(true);
    CHECK(playing->RaycastCursor().status == Status::InputBlocked);
    CHECK(playing->RaycastScreen({10, 10}).status == Status::Miss);
    playing->GetInput().SetPointerBlocked(false);
    auto hud = std::make_unique<UIComponent>();
    hud->AddButton({UIAnchor::TopLeft, 0, 0, 50, 50}, "", [] {});
    std::unique_ptr<GameComponent> hudObject = std::move(hud);
    auto hudId = playing->GetScene().AddComponent("HUD", hudObject).id;
    CHECK(api.frame(api.context, 0) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::InputBlocked);
    CHECK(playing->GetScene().RemoveComponent(hudId) == SceneResult::Ok);
    CHECK(api.frame(api.context, 0) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::Miss);
    CHECK(api.resizeViewport(api.context, 640, 400) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::InvalidViewport);
    CHECK(api.frame(api.context, 0) == EngineSceneOk);
    CHECK(cursor.set(cursor.context, 320.5f, 200.5f, 640, 400, 1) == EngineSceneOk);
    CHECK(playing->RaycastCursor().status == Status::Miss);
    playing->DebugDrawTrace(ray.ray, {}, .01f);
    CHECK(playing->GetDebugTraceLineCount() == 1);
    Sleep(20);
    CHECK(playing->GetDebugTraceLineCount() == 0);
    Result hit{Status::Hit, Hit{0, {0, 0, 0}, {0, 1, 0}, 1}};
    playing->DebugDrawTrace(ray.ray, hit, 2);
    CHECK(playing->GetDebugTraceLineCount() == 5);
    playing->ClearDebugTraces();
    CHECK(playing->GetDebugTraceLineCount() == 0);
    CHECK(api.control(api.context, EngineStop) == EngineSceneOk);
    playing = nullptr;
    Game standalone;
    standalone.GetSettings().SetVSync(false);
    CHECK(standalone.InitializeRuntimeWindow(false));
    CHECK(standalone.ScreenPointToRay({10.25f, 10.75f})); // before the first camera/frame update
    const auto beforeMove = standalone.ScreenPointToRay({10.25f, 10.75f}).ray;
    standalone.GetPlayer()->SetPosition(standalone.GetPlayer()->GetPosition() + glm::vec3(3, 0, 0));
    CHECK(Near(standalone.ScreenPointToRay({10.25f, 10.75f}).ray.origin, beforeMove.origin + glm::vec3(3, 0, 0)));
    standalone.GetPlayer()->SetRotate(0, 0);
    const auto middle =
        Trace::ViewportPoint{standalone.GetDisplay()->GetWidth() * .5f, standalone.GetDisplay()->GetHeight() * .5f};
    const auto turned=standalone.ScreenPointToRay(middle);
    if(!turned||!Near(turned.ray.direction,{1,0,0}))throw std::runtime_error(std::string("turned ray: ")+Trace::StatusName(turned.status)+" "+std::to_string(turned.ray.direction.x)+","+std::to_string(turned.ray.direction.y)+","+std::to_string(turned.ray.direction.z)+" forward "+std::to_string(standalone.GetPlayer()->GetForward().x));
    CHECK(standalone.RunRuntimeFrame(0) == Game::RuntimeFrameResult::Running);
    CHECK(standalone.ScreenPointToRay({10.25f, 10.75f}));
    CHECK(standalone.RaycastScreen({10.25f, 10.75f}).status == Status::Miss);
    standalone.DebugDrawTrace({{0, 0, 0}, {0, 1, 0}, 2}, {}, 1);
    CHECK(standalone.RunRuntimeFrame(0) == Game::RuntimeFrameResult::Running);
    CHECK(standalone.GetDebugTraceLineCount() == 1);
    standalone.GetSettings().RequestWindowClose();
    standalone.GetSettings().RequestWindowClose();
    CHECK(standalone.RunRuntimeFrame(0) == Game::RuntimeFrameResult::Closed);
}
