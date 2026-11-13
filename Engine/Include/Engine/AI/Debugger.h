#pragma once
#include <Engine/AI/AiTypes.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Game;

namespace Ai
{
class World;
class Controller;

struct DebugLine
{
    glm::vec3 from{0}, to{0};
    uint32_t rgba = 0xffffffff;
};

/**
 * What the AI is doing, like Unreal Gameplay Debugger: paths, sight cones, links to targets,
 * and a text report of the selected controller (state, tasks, memory, movement, history).
 * Lines go to the game's overlay; text goes wherever the game shows it (HUD, log).
 */
class Debugger
{
public:
    explicit Debugger(World& world) : world_(world) {}
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }
    void Select(Controller& controller);
    void ClearSelection() { selected_ = 0; }
    Controller* Selected() const;
    /** Lines of the last step; empty while hidden. */
    const std::vector<DebugLine>& Lines() const { return lines_; }
    /** State, tasks, memory, movement, senses and recent history of one controller. */
    std::string Describe(const Controller& controller) const;
    /** One line per controller: name, state, movement. */
    std::string Overview() const;
    /** Sends the lines to the game overlay; call once per frame after the AI step (the engine does it). */
    void Draw(Game& game) const;
    /** Rebuilds the lines; called by World::Tick. */
    void Update();

private:
    World& world_;
    bool visible_ = false;
    uint32_t selected_ = 0;
    std::vector<DebugLine> lines_;
};
} // namespace Ai
