#pragma once
#include <Engine/AI/AiTypes.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Ai
{
/** Something a unit knows about: who, where it was last noticed, how long ago. */
struct Perceived
{
    ObjectRef who;
    glm::vec3 lastPosition{0};
    float age = 0;              // seconds since last noticed
    bool visibleNow = false;
    float distance = 0;         // from the controller when last noticed
};

/** One sensory event delivered to Controller::OnPerception. */
struct Stimulus
{
    enum class Kind { Sight, Hearing, Damage } kind = Kind::Sight;
    ObjectRef source;
    glm::vec3 position{0};
    float strength = 0;          // loudness for hearing, amount for damage
    bool lost = false;           // sight: the source stopped being visible
};

/** Sight, hearing and damage of one controller, like Unreal AI Perception. */
class Perception
{
public:
    void SetTeam(Team team) { team_ = team; }
    Team GetTeam() const { return team_; }
    /** fieldOfViewDegrees is the whole cone; 360 sees all around. memorySeconds: how long a lost target is remembered. */
    void EnableSight(float radius, float fieldOfViewDegrees, float memorySeconds = 5);
    void DisableSight() { sightRadius_ = 0; }
    void EnableHearing(float radius);
    /** The best known hostile is written here; the key is cleared when nothing is known. */
    void WriteTargetTo(Key<ObjectRef> key) { targetKey_ = key.name ? key.name : ""; }
    /** Where the best target was last noticed. */
    void WriteLastSeenTo(Key<glm::vec3> key) { lastSeenKey_ = key.name ? key.name : ""; }
    const std::vector<Perceived>& Known() const { return known_; }
    /** Visible hostiles first, nearest first; then the most recently noticed. Null when nothing is known. */
    const Perceived* Best() const;
    bool CanSee(ObjectRef who) const;
    bool SightEnabled() const { return sightRadius_ > 0; }
    float SightRadius() const { return sightRadius_; }
    float FieldOfView() const { return fieldOfView_; }
    float HearingRadius() const { return hearingRadius_; }
    float Memory() const { return memory_; }

private:
    friend class World;
    friend class Controller;
    Team team_{};
    float sightRadius_ = 0, fieldOfView_ = 120, memory_ = 5, hearingRadius_ = 0;
    std::string targetKey_;
    std::string lastSeenKey_;
    std::vector<Perceived> known_;
    float sinceUpdate_ = 0;
    bool anyVisible_ = false;
};
} // namespace Ai
