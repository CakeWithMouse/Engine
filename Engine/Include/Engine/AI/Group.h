#pragma once
#include <Engine/AI/Controller.h>
#include <Engine/AI/Navigation.h>
#include <memory>
#include <vector>

class Character;

namespace Ai
{
/** Rows of slots at the destination, facing the order direction. */
struct Formation
{
    float spacing = 4;
    int columns = 0;   // 0 picks a roughly square block
};

/**
 * A squad: one controller and one state machine for many soldiers. MoveTo uses a shared flow
 * field instead of a path per soldier; soldiers leave the field for their slot once it is in
 * sight. Soldiers are light records; their bodies are MeshInstance copies or Characters.
 */
class Group : public Controller
{
public:
    struct Member
    {
        ComponentId batch = 0;        // InstancedMeshComponent with the soldier
        size_t index = 0;             // instance index in the batch
        ComponentId character = 0;    // or a Character
        glm::vec2 position{0}, velocity{0}, slot{0};
        float height = 0, radius = 1, speed = 5, yaw = 0;
        bool hasSlot = false, settled = true, alive = true;
        bool direct = false;          // the straight line to the slot was clear: no flow field needed
    };

    explicit Group(World& world);
    void AddInstance(ComponentId batch, size_t index, float radius, float speed);
    void AddCharacter(Character& character, float radius = 0.4f);
    /** Removes a soldier, for example when it dies. The instance is hidden. */
    void RemoveMember(size_t member);
    size_t Size() const;
    const std::vector<Member>& Members() const { return members_; }
    void SetFacing(float yaw) { facing_ = yaw; }
    float Facing() const { return facing_; }
    void SetFormation(Formation formation) { formation_ = formation; }
    /** Nearest group of a hostile team whose centre is within radius; null when none. */
    Group* FindHostile(float radius) const;
    /** Share of soldiers that must stand in their slots for the order to count as done. */
    void SetArrivedFraction(float fraction) { arrivedFraction_ = fraction; }

    using Controller::MoveTo;
    void MoveTo(glm::vec3 point, float acceptanceRadius = 0.5f) override;
    void MoveTo(ObjectRef target, float acceptanceRadius = 1.5f) override;
    void StopMovement() override;
    void SerializeSaveGame(SaveArchive& archive) override;

protected:
    void UpdateMovement(float dt) override;
    void WriteBody(float dt) override;
    bool SyncBody() override;

private:
    friend class World;
    void AssignSlots(glm::vec2 center);
    void RequestField();
    void Recount();
    Formation formation_;
    std::vector<Member> members_;
    std::shared_ptr<const Detail::FlowField> field_;
    float facing_ = 0, arrivedFraction_ = 0.95f, orderTime_ = 0;
    bool moving_ = false, reloadPositions_ = false, fieldChecked_ = false;
    float fieldMargin_ = 64;
    size_t alive_ = 0;        // living soldiers, kept by Add*, RemoveMember, SyncBody and loading
    float largest_ = 0;       // the largest radius among them
};
} // namespace Ai
