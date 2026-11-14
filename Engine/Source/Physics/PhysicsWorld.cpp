#include <Engine/Physics/PhysicsWorld.h>
#include <Engine/Physics/PhysicsSettings.h>
#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_map>

#ifdef ENGINE_ENABLE_JOLT

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace
{
// Two object layers: 0 = static/kinematic, 1 = dynamic. Everything collides with everything.
constexpr JPH::ObjectLayer LayerNonMoving = 0;
constexpr JPH::ObjectLayer LayerMoving = 1;

constexpr JPH::BroadPhaseLayer BroadPhaseNonMoving(0);
constexpr JPH::BroadPhaseLayer BroadPhaseMoving(1);

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return layer == LayerMoving ? BroadPhaseMoving : BroadPhaseNonMoving;
    }
    unsigned int GetNumBroadPhaseLayers() const override { return 2; }
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
};

void EnsureJoltGlobalsInitialized()
{
    static struct Globals
    {
        Globals()
        {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
        ~Globals()
        {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    } globals;
    (void)globals;
}

JPH::Vec3 ToJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
JPH::Quat ToJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
glm::vec3 FromJolt(JPH::Vec3Arg v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
glm::quat FromJolt(JPH::QuatArg q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }
} // namespace

struct PhysicsWorld::Impl
{
    JPH::TempAllocatorImpl tempAllocator{10 * 1024 * 1024};
    JPH::JobSystemThreadPool jobSystem{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                       static_cast<int>(std::thread::hardware_concurrency()) - 1};
    JPH::PhysicsSystem system;
    BroadPhaseLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;
    std::unordered_map<uint32_t, JPH::BodyID> bodies;
    uint32_t nextHandle = 1;
};

PhysicsWorld::PhysicsWorld() = default;
PhysicsWorld::~PhysicsWorld()
{
    Shutdown();
}

bool PhysicsWorld::Initialize(const PhysicsSettings& settings, std::string& error)
{
    if (impl_) return true;
    try
    {
        EnsureJoltGlobalsInitialized();
        settings_ = settings;
        impl_ = std::make_unique<Impl>();
        impl_->system.Init(1024, 0, 1024, 1024, impl_->broadPhaseLayerInterface,
                           impl_->objectVsBroadPhaseFilter, impl_->objectLayerPairFilter);
        impl_->system.SetGravity(ToJolt(settings_.GetGravity()));
        JPH::PhysicsSettings physicsSettings = impl_->system.GetPhysicsSettings();
        physicsSettings.mNumVelocitySteps = static_cast<unsigned int>(settings_.GetVelocitySteps());
        physicsSettings.mNumPositionSteps = static_cast<unsigned int>(settings_.GetPositionSteps());
        impl_->system.SetPhysicsSettings(physicsSettings);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        impl_.reset();
        return false;
    }
}

void PhysicsWorld::Shutdown()
{
    if (!impl_) return;
    impl_.reset();
    accumulator_ = 0.0f;
}

bool PhysicsWorld::IsInitialized() const
{
    return impl_ != nullptr;
}

uint32_t PhysicsWorld::CreateBody(const PhysicsBodyDesc& desc)
{
    if (!impl_ || !std::isfinite(desc.density) || desc.density <= 0.0f) return 0;
    try
    {
        JPH::Ref<JPH::ConvexShape> shape;
        switch (desc.shape)
        {
        case PhysicsShapeType::Box:
            shape = new JPH::BoxShape(ToJolt(glm::abs(desc.halfExtents)));
            break;
        case PhysicsShapeType::Sphere:
            shape = new JPH::SphereShape(std::abs(desc.radius));
            break;
        case PhysicsShapeType::Capsule:
            shape = new JPH::CapsuleShape(std::abs(desc.halfHeight), std::abs(desc.radius));
            break;
        default:
            return 0;
        }
        // Set density before Jolt derives the body's mass and inertia from the shape.
        shape->SetDensity(desc.density);
        JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
        JPH::ObjectLayer layer = LayerMoving;
        if (desc.type == PhysicsBodyType::Static)
        {
            motionType = JPH::EMotionType::Static;
            layer = LayerNonMoving;
        }
        else if (desc.type == PhysicsBodyType::Kinematic)
        {
            motionType = JPH::EMotionType::Kinematic;
            layer = LayerNonMoving;
        }

        JPH::BodyCreationSettings settings(shape, JPH::RVec3(ToJolt(desc.position)), ToJolt(desc.rotation),
                                           motionType, layer);
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mAllowSleeping = desc.allowSleeping;

        JPH::BodyID bodyId = impl_->system.GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::Activate);
        if (!bodyId.IsInvalid())
        {
            const uint32_t handle = impl_->nextHandle++;
            impl_->bodies.emplace(handle, bodyId);
            return handle;
        }
    }
    catch (const std::exception&)
    {
    }
    return 0;
}

void PhysicsWorld::DestroyBody(uint32_t handle)
{
    if (!impl_) return;
    const auto it = impl_->bodies.find(handle);
    if (it == impl_->bodies.end()) return;
    impl_->system.GetBodyInterface().RemoveBody(it->second);
    impl_->system.GetBodyInterface().DestroyBody(it->second);
    impl_->bodies.erase(it);
}

bool PhysicsWorld::IsValid(uint32_t handle) const
{
    return impl_ && impl_->bodies.find(handle) != impl_->bodies.end();
}

uint32_t PhysicsWorld::GetBodyCount() const
{
    return impl_ ? static_cast<uint32_t>(impl_->bodies.size()) : 0;
}

glm::vec3 PhysicsWorld::GetPosition(uint32_t handle) const
{
    if (!IsValid(handle)) return glm::vec3(0.0f);
    return FromJolt(JPH::Vec3(impl_->system.GetBodyInterface().GetPosition(impl_->bodies.at(handle))));
}

glm::quat PhysicsWorld::GetRotation(uint32_t handle) const
{
    if (!IsValid(handle)) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return FromJolt(impl_->system.GetBodyInterface().GetRotation(impl_->bodies.at(handle)));
}

void PhysicsWorld::SetPositionAndRotation(uint32_t handle, const glm::vec3& position, const glm::quat& rotation)
{
    if (!IsValid(handle)) return;
    impl_->system.GetBodyInterface().SetPositionAndRotation(impl_->bodies.at(handle),
                                                            JPH::RVec3(ToJolt(position)), ToJolt(rotation),
                                                            JPH::EActivation::Activate);
}

glm::vec3 PhysicsWorld::GetLinearVelocity(uint32_t handle) const
{
    if (!IsValid(handle)) return glm::vec3(0.0f);
    return FromJolt(impl_->system.GetBodyInterface().GetLinearVelocity(impl_->bodies.at(handle)));
}

void PhysicsWorld::SetLinearVelocity(uint32_t handle, const glm::vec3& velocity)
{
    if (!IsValid(handle)) return;
    impl_->system.GetBodyInterface().SetLinearVelocity(impl_->bodies.at(handle), ToJolt(velocity));
}

void PhysicsWorld::AddImpulse(uint32_t handle, const glm::vec3& impulse)
{
    if (!IsValid(handle)) return;
    // A raw Jolt impulse does not wake a sleeping body; this wrapper does.
    impl_->system.GetBodyInterface().ActivateBody(impl_->bodies.at(handle));
    impl_->system.GetBodyInterface().AddImpulse(impl_->bodies.at(handle), ToJolt(impulse));
}

void PhysicsWorld::SetActive(uint32_t handle, bool active)
{
    if (!IsValid(handle)) return;
    impl_->system.GetBodyInterface().ActivateBody(impl_->bodies.at(handle));
}

void PhysicsWorld::Update(float deltaTime)
{
    if (!impl_ || deltaTime <= 0.0f) return;
    const float step = settings_.GetFixedTimeStep();
    const int maxSteps = settings_.GetMaxSubSteps();
    accumulator_ = std::min(accumulator_ + deltaTime, step * maxSteps);
    int steps = 0;
    while (accumulator_ >= step && steps < maxSteps)
    {
        accumulator_ -= step;
        ++steps;
    }
    if (steps > 0)
        impl_->system.Update(step * steps, steps, &impl_->tempAllocator, &impl_->jobSystem);
}

const PhysicsSettings& PhysicsWorld::GetSettings() const
{
    return settings_;
}

void PhysicsWorld::ApplySettings(const PhysicsSettings& settings)
{
    settings_ = settings;
    if (!impl_) return;
    impl_->system.SetGravity(ToJolt(settings_.GetGravity()));
    JPH::PhysicsSettings physicsSettings = impl_->system.GetPhysicsSettings();
    physicsSettings.mNumVelocitySteps = static_cast<unsigned int>(settings_.GetVelocitySteps());
    physicsSettings.mNumPositionSteps = static_cast<unsigned int>(settings_.GetPositionSteps());
    impl_->system.SetPhysicsSettings(physicsSettings);
}

bool PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
                           PhysicsRaycastHit& hit) const
{
    hit = PhysicsRaycastHit{};
    if (!impl_) return false;
    const JPH::RRayCast ray{JPH::RVec3(ToJolt(origin)), JPH::Vec3(ToJolt(direction)) * maxDistance};
    JPH::RayCastResult result;
    if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, result)) return false;
    hit.position = FromJolt(JPH::Vec3(ray.GetPointOnRay(result.mFraction)));
    hit.distance = maxDistance * result.mFraction;
    // The ray cast does not report a surface normal; recover it from the hit shape.
    JPH::BodyLockRead lock(impl_->system.GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded())
    {
        const JPH::Body& body = lock.GetBody();
        const JPH::Vec3 localPoint = body.GetInverseCenterOfMassTransform() * JPH::Vec3(ToJolt(hit.position));
        hit.normal = FromJolt(body.GetRotation() * body.GetShape()->GetSurfaceNormal(result.mSubShapeID2, localPoint));
    }
    for (const auto& entry : impl_->bodies)
        if (entry.second == result.mBodyID) { hit.body = entry.first; break; }
    return true;
}

#else // !ENGINE_ENABLE_JOLT

// Stub build: the engine was compiled without vcpkg/Jolt. The world stays
// uninitialized and every operation is a documented no-op.
struct PhysicsWorld::Impl {};
PhysicsWorld::PhysicsWorld() = default;
PhysicsWorld::~PhysicsWorld() = default;
bool PhysicsWorld::Initialize(const PhysicsSettings&, std::string& error)
{
    error = "Engine built without Jolt physics (vcpkg dependency 'joltphysics' missing).";
    return false;
}
void PhysicsWorld::Shutdown() {}
bool PhysicsWorld::IsInitialized() const { return false; }
uint32_t PhysicsWorld::CreateBody(const PhysicsBodyDesc&) { return 0; }
void PhysicsWorld::DestroyBody(uint32_t) {}
bool PhysicsWorld::IsValid(uint32_t) const { return false; }
uint32_t PhysicsWorld::GetBodyCount() const { return 0; }
glm::vec3 PhysicsWorld::GetPosition(uint32_t) const { return glm::vec3(0.0f); }
glm::quat PhysicsWorld::GetRotation(uint32_t) const { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); }
void PhysicsWorld::SetPositionAndRotation(uint32_t, const glm::vec3&, const glm::quat&) {}
glm::vec3 PhysicsWorld::GetLinearVelocity(uint32_t) const { return glm::vec3(0.0f); }
void PhysicsWorld::SetLinearVelocity(uint32_t, const glm::vec3&) {}
void PhysicsWorld::AddImpulse(uint32_t, const glm::vec3&) {}
void PhysicsWorld::SetActive(uint32_t, bool) {}
void PhysicsWorld::Update(float) {}
const PhysicsSettings& PhysicsWorld::GetSettings() const { return settings_; }
void PhysicsWorld::ApplySettings(const PhysicsSettings& settings) { settings_ = settings; }
bool PhysicsWorld::Raycast(const glm::vec3&, const glm::vec3&, float, PhysicsRaycastHit&) const { return false; }

#endif // ENGINE_ENABLE_JOLT
