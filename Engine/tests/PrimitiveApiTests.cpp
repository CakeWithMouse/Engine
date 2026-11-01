#include "../Source/Components/ComponentInternal.h"
#include <Engine/Components/SpecificComponents/CubeComponent.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/SpecificComponents/SphereComponent.h>
#include <Engine/Components/SpecificComponents/TriangleComponent.h>
#include <Engine/Components/SpecificComponents/FBXComponent.h>
#include <Engine/Components/SpecificComponents/MeshComponent.h>
#include <Engine/Components/SpecificComponents/InstancedMeshComponent.h>
#include <Engine/Components/SpecificComponents/SkeletalMeshComponent.h>
#include <Engine/Render/MeshPrimitives.h>
#include <Engine/Components/ProjectObject.h>
#include <type_traits>
#include <cmath>
#include <stdexcept>
namespace {
#define HIDDEN_METHOD_TRAIT(Name) \
template<class T, class = void> struct CanCall##Name : std::false_type {}; \
template<class T> struct CanCall##Name<T, std::void_t<decltype(&T::Name)>> : std::true_type {};
HIDDEN_METHOD_TRAIT(InitCube)
HIDDEN_METHOD_TRAIT(InitSphere)
HIDDEN_METHOD_TRAIT(InitSquare)
HIDDEN_METHOD_TRAIT(InitSquareWithColor)
HIDDEN_METHOD_TRAIT(InitPoints)
HIDDEN_METHOD_TRAIT(GetFrameIndex)
HIDDEN_METHOD_TRAIT(GetWorldMatrix)
HIDDEN_METHOD_TRAIT(SetParentWithoutScale)
HIDDEN_METHOD_TRAIT(SetShaderNames)
HIDDEN_METHOD_TRAIT(SetReflectionSettings)
HIDDEN_METHOD_TRAIT(SetStaticShadowCaster)
HIDDEN_METHOD_TRAIT(RemapProjectReferences)
#undef HIDDEN_METHOD_TRAIT
class UserCube : public CubeComponent {};
class UserGame : public Game {};
struct IndependentObject {};
static_assert(!CanCallGetWorldMatrix<GameComponent>::value && !CanCallGetWorldMatrix<UserCube>::value);
static_assert(!CanCallSetParentWithoutScale<GameComponent>::value && !CanCallSetParentWithoutScale<UserCube>::value);
static_assert(!CanCallSetShaderNames<GameComponent>::value && !CanCallSetShaderNames<UserCube>::value);
static_assert(!CanCallSetReflectionSettings<GameComponent>::value && !CanCallSetReflectionSettings<UserCube>::value);
static_assert(!CanCallSetStaticShadowCaster<GameComponent>::value && !CanCallSetStaticShadowCaster<UserCube>::value);
static_assert(!CanCallRemapProjectReferences<GameComponent>::value && !CanCallRemapProjectReferences<UserCube>::value);
static_assert(!CanCallRemapProjectReferences<Character>::value && !CanCallRemapProjectReferences<PlayerCharacter>::value);
static_assert(!CanCallRemapProjectReferences<PlaySettings>::value && !CanCallRemapProjectReferences<ProjectObjectComponent<IndependentObject>>::value);
static_assert(std::is_base_of_v<GameComponent,Character> && !std::is_base_of_v<CubeComponent,Character>);
static_assert(!CanCallGetFrameIndex<Game>::value && !CanCallGetFrameIndex<UserGame>::value);
static_assert(!CanCallInitCube<CubeComponent>::value && !CanCallInitCube<UserCube>::value);
static_assert(!CanCallInitSphere<SphereComponent>::value);
static_assert(!CanCallInitSquare<TriangleComponent>::value && !CanCallInitSquareWithColor<TriangleComponent>::value);
static_assert(!CanCallInitPoints<GameComponent>::value && !CanCallInitPoints<FBXComponent>::value);
void Require(bool yes) { if(!yes) throw std::runtime_error("Ready primitive API failed"); }
}
void PrimitiveApiTests()
{
    CubeComponent cube;
    CubeComponent transformed({0,0,0},{0,0,0},{1,1,1});
    CubeComponent colored({0,0,0},{0,0,0},{1,1,1},{1,1,1,1});
    SphereComponent sphere;
    TriangleComponent square;
    Require(cube.GetIndexCount()==36 && transformed.GetIndexCount()==36 && colored.GetIndexCount()==36);
    Require(sphere.GetIndexCount()>0 && square.GetIndexCount()==6);
    Trace::Statistics stats;
    Require(bool(cube.RaycastGeometry({{0,0,5},{0,0,-1},10},{},stats)));
    Require(bool(sphere.RaycastGeometry({{0,0,5},{0,0,-1},10},{},stats)));
    Scene scene;
    std::unique_ptr<GameComponent> object = std::make_unique<GameComponent>();
    auto* owner=object.get();
    owner->SetPrimitive(MeshPrimitives::Cube(2));
    const auto added=scene.AddComponent("ready",object);
    Require(added.result==SceneResult::Ok);
    auto hit=scene.Raycast({{0,0,5},{0,0,-1},10});
    Require(bool(hit) && hit.hit->componentId==added.id && std::abs(hit.hit->distance-4)<1e-4f);
    auto* visual=dynamic_cast<MeshComponent*>(owner->GetRenderComponent());
    Require(visual && visual->GetMesh().Vertices.size()==24);
    Require(owner->GetModelAsset().empty());
    auto* instance=owner->CreateInstance({4,0,0},{0,0,0},{1,1,1},{1,0,0,1});
    owner->Update();
    Require(owner->GetRenderComponent()->GetInstanceCount()==1);
    Require(!scene.Raycast({{0,0,5},{0,0,-1},10}));
    hit=scene.Raycast({{4,0,5},{0,0,-1},10});
    Require(bool(hit) && hit.hit->instanceIndex==0);
    instance->SetPosition({8,0,0});
    owner->Update();
    Require(bool(scene.Raycast({{8,0,5},{0,0,-1},10})));
    owner->SetPrimitive(MeshPrimitives::Sphere(0.5f,16,8));
    hit=scene.Raycast({{8,0,5},{0,0,-1},10});
    Require(bool(hit) && std::abs(hit.hit->distance-4.5f)<1e-4f);
    bool rejected=false;
    try { owner->SetPrimitive({}); } catch(const std::invalid_argument&) { rejected=true; }
    Require(rejected && bool(scene.Raycast({{8,0,5},{0,0,-1},10})));
    auto invalid=MeshPrimitives::Cube(); invalid.Indices[0]=UINT32_MAX;
    rejected=false;
    try { owner->SetPrimitive(std::move(invalid)); } catch(const std::invalid_argument&) { rejected=true; }
    Require(rejected && bool(scene.Raycast({{8,0,5},{0,0,-1},10})));
    EngineInternal::ComponentAccess::Shaders(*owner, "legacy-vs", "legacy-ps");
    Require(owner->GetRenderComponent()==owner);
    EngineInternal::ComponentAccess::Shaders(*owner, {},{});
    Require(owner->GetRenderComponent()!=owner);
    MeshComponent mesh;
    mesh.SetPrimitive(MeshPrimitives::Plane(4,6));
    Require(mesh.GetMesh().Indices.size()==6 && mesh.GetRenderComponent()==&mesh);
    square.SetPrimitive(MeshPrimitives::Cube());
    Require(square.GetIndexCount()==36);
    InstancedMeshComponent instanced;
    SkeletalMeshComponent skeletal;
    for(GameComponent* component : {static_cast<GameComponent*>(&instanced), static_cast<GameComponent*>(&skeletal)}) {
        component->SetPrimitive(MeshPrimitives::Cube(2));
        auto result=component->RaycastGeometry({{0,0,5},{0,0,-1},10},{},stats);
        Require(bool(result) && std::abs(result.hit->distance-4)<1e-4f);
    }
}
