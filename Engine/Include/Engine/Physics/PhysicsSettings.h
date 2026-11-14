#pragma once
#include <glm/glm.hpp>

// Simulation settings of the physics world. Plain data; applied on the next
// PhysicsWorld::Update/ApplySettings call. Obtain by reference through
// Game::GetSettings().Physics(); not serialized into scenes automatically.
class PhysicsSettings
{
public:
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    const glm::vec3& GetGravity() const { return gravity_; }
    void SetGravity(const glm::vec3& gravity) { gravity_ = gravity; }

    // Fixed simulation step in seconds (default 1/60).
    float GetFixedTimeStep() const { return fixedTimeStep_; }
    void SetFixedTimeStep(float seconds) { fixedTimeStep_ = seconds > 0.0f ? seconds : fixedTimeStep_; }

    // Upper bound of fixed steps simulated per Update call (spiral-of-death guard).
    int GetMaxSubSteps() const { return maxSubSteps_; }
    void SetMaxSubSteps(int steps) { maxSubSteps_ = steps > 0 ? steps : maxSubSteps_; }

    int GetVelocitySteps() const { return velocitySteps_; }
    void SetVelocitySteps(int steps) { velocitySteps_ = steps > 0 ? steps : velocitySteps_; }
    int GetPositionSteps() const { return positionSteps_; }
    void SetPositionSteps(int steps) { positionSteps_ = steps > 0 ? steps : positionSteps_; }

private:
    bool enabled_ = true;
    glm::vec3 gravity_ = glm::vec3(0.0f, -9.81f, 0.0f);
    float fixedTimeStep_ = 1.0f / 60.0f;
    int maxSubSteps_ = 5;
    int velocitySteps_ = 10;
    int positionSteps_ = 2;
};
