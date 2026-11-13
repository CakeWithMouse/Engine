// Stage 6: what the debugger shows: lines of paths, sight and targets, text reports, overlay.
#include "TestSupport.h"
#include <algorithm>

namespace
{
const Ai::Key<glm::vec3> Goal{"Goal"};
const Ai::Key<Ai::ObjectRef> Target{"Target"};
constexpr uint32_t Green = 0x40ff60ff, Blue = 0x50a0ffff, Red = 0xff4040ff;

size_t Count(const std::vector<Ai::DebugLine>& lines, uint32_t colour)
{
    return static_cast<size_t>(std::count_if(lines.begin(), lines.end(), [&](const Ai::DebugLine& l) { return l.rgba == colour; }));
}
} // namespace

void DebuggerTests()
{
    Game game;
    NavVolume& bounds = AddToScene<NavVolume>(game, "Bounds");
    bounds.mode = 0;
    bounds.SetScale({40, 1, 40});
    NavVolume& wall = AddToScene<NavVolume>(game, "Wall", {0, 0, -3});
    wall.SetScale({1, 1, 20});
    Character& scout = AddToScene<Character>(game, "Scout", {-8, 0, -8});
    Character& enemy = AddToScene<Character>(game, "Enemy", {-8, 0, 2});
    game.InitializeGameplay();
    Ai::World& world = Ai::Get(game);
    world.AddTarget(enemy, {1});
    Ai::Controller& ai = world.Spawn(scout);
    ai.GetPerception().SetTeam({2});
    ai.GetPerception().EnableSight(15, 360);
    ai.GetPerception().WriteTargetTo(Target);
    Ai::MachineBuilder m("Scout");
    m.State("Go").Task<Ai::Tasks::MoveTo>(Goal, 0.5f);
    ai.Run(m.Build());
    ai.Memory().Set(Goal, glm::vec3(8, 0, -8));

    Step(game, 0.3f);
    CHECK(world.Debug().Lines().empty());                    // hidden by default: no work
    world.Debug().SetVisible(true);
    world.Debug().Select(ai);
    Step(game, 0.3f);
    const auto& lines = world.Debug().Lines();
    CHECK(Count(lines, Green) >= 2);                          // the path goes around the wall: two corners at least
    CHECK(Count(lines, Blue) >= 12);                          // the sight circle
    CHECK(Count(lines, Red) == 1);                            // the visible enemy
    CHECK(world.Debug().Selected() == &ai);

    const std::string report = world.Debug().Describe(ai);
    CHECK(report.find("state: Go") != std::string::npos);
    CHECK(report.find("MoveTo Goal") != std::string::npos);
    CHECK(report.find("movement: Moving") != std::string::npos);
    CHECK(report.find("Goal = (8, 0, -8)") != std::string::npos);
    CHECK(report.find("(visible)") != std::string::npos);
    CHECK(report.find("history:") != std::string::npos);
    CHECK(world.Debug().Overview().find("Go  Moving") != std::string::npos);

    world.Debug().Draw(game);                                  // what Game::PrepareRenderFrame does each frame
    CHECK(game.GetFrameDebugLineCount() == lines.size());
    world.Debug().SetVisible(false);
    Step(game, 0.1f);
    const size_t before = game.GetFrameDebugLineCount();
    world.Debug().Draw(game);
    CHECK(game.GetFrameDebugLineCount() == before);            // hidden: nothing more is drawn
}
