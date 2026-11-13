// AI checks without window and GPU. A filter argument runs only tests whose name contains it.
#include "TestSupport.h"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>

void BlackboardTests();
void MachineOrderTests();
void MachineSelectionTests();
void MachineTransitionOrderTests();
void MachineCompletionTests();
void MachineEventTests();
void MachineDelayTests();
void MachineLinkedTests();
void MachineTargetsTests();
void MachineBuildErrorTests();
void MachineDataIsPerUnitTests();
void ServiceAndIntervalTests();
void ExitOnDestroyTests();
void NavGridBasicsTests();
void NavPathAroundWallTests();
void NavWideUnitTests();
void NavCornerCuttingTests();
void NavUnreachableTests();
void NavCostTests();
void NavSightTests();
void NavServiceTests();
void NavClearanceTests();
void SubsystemAvailabilityTests();
void RouteAndRestTests();
void PathAroundVolumesTests();
void ChaseMovingTargetTests();
void UnreachableAndStuckTests();
void BodyRemovedTests();
void TurretAndPlayerTests();
void SightTests();
void LostAndMemoryTests();
void TeamTests();
void HearingDamageBroadcastTests();
void PerceptionBudgetTests();
void GuideStartsOnPatrolTests();
void GuideGuardScenarioTests();
void SaveAndLoadTests();
void DebuggerTests();
void QueryTests();
void HeadOnTests();
void CrowdToOnePointTests();
void StandingUnitIsNotPushedTests();
void SquadThroughGapTests();
void SharedFieldAndOpenFieldTests();
void SquadMachineAndTeamsTests();
void WindowedFieldTests();
void StaggeredReplanTests();
void MachineRegistryTests();
void SmartObjectTests();
int RunBench(int soldiers, float seconds, bool profile);

int main(int argc, char** argv)
{
    // --bench [soldiers] [seconds] [--profile]: the stage 10 measurement instead of the tests.
    if (argc > 1 && std::strcmp(argv[1], "--bench") == 0)
    {
        const bool profile = std::strcmp(argv[argc - 1], "--profile") == 0;
        const int numbers = argc - 2 - (profile ? 1 : 0);
        return RunBench(numbers > 0 ? std::atoi(argv[2]) : 30000, numbers > 1 ? static_cast<float>(std::atof(argv[3])) : 60.0f, profile);
    }
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"stage1 blackboard values and types", BlackboardTests},
        {"stage1 enter, tick and exit order", MachineOrderTests},
        {"stage1 parent selects its first allowed child", MachineSelectionTests},
        {"stage1 transitions leaf first, one per think", MachineTransitionOrderTests},
        {"stage1 completion bubbles up and restarts", MachineCompletionTests},
        {"stage1 events wake and are used once", MachineEventTests},
        {"stage1 delayed transitions and cancel", MachineDelayTests},
        {"stage1 linked machine and Finish", MachineLinkedTests},
        {"stage1 Next and Finish targets", MachineTargetsTests},
        {"stage1 readable build errors", MachineBuildErrorTests},
        {"stage1 task data is per unit", MachineDataIsPerUnitTests},
        {"stage1 services and think interval", ServiceAndIntervalTests},
        {"stage1 exit on stop and destroy", ExitOnDestroyTests},
        {"stage2 grid basics", NavGridBasicsTests},
        {"stage2 clearance to walls and edges", NavClearanceTests},
        {"stage2 path around a wall", NavPathAroundWallTests},
        {"stage2 wide unit and narrow gap", NavWideUnitTests},
        {"stage2 no corner cutting", NavCornerCuttingTests},
        {"stage2 unreachable goals", NavUnreachableTests},
        {"stage2 costs and straightening", NavCostTests},
        {"stage2 line of sight", NavSightTests},
        {"stage2 time-sliced path service", NavServiceTests},
        {"stage3 built-in subsystem only in gameplay", SubsystemAvailabilityTests},
        {"stage3 route with rest stops", RouteAndRestTests},
        {"stage3 path around NavVolume walls", PathAroundVolumesTests},
        {"stage3 chase a moving target", ChaseMovingTargetTests},
        {"stage3 unreachable goal and stuck body", UnreachableAndStuckTests},
        {"stage3 removed body removes the controller", BodyRemovedTests},
        {"stage4 sight: radius, field of view, walls", SightTests},
        {"stage4 lost target and memory", LostAndMemoryTests},
        {"stage4 teams and attitudes", TeamTests},
        {"stage4 hearing, damage and broadcast", HearingDamageBroadcastTests},
        {"stage4 sight checks within the budget", PerceptionBudgetTests},
        {"stage4 guide: GuardStartsOnPatrol", GuideStartsOnPatrolTests},
        {"stage4 guide: guard patrols, chases, hits, searches", GuideGuardScenarioTests},
        {"stage5 save and load through SaveGame", SaveAndLoadTests},
        {"stage6 debugger lines, reports and overlay", DebuggerTests},
        {"stage7 queries: generators, filters, scores, task", QueryTests},
        {"stage8 head-on units pass each other", HeadOnTests},
        {"stage8 crowd to one point: no overlap, no jitter", CrowdToOnePointTests},
        {"stage8 a standing unit is walked around", StandingUnitIsNotPushedTests},
        {"stage9 squad of 100 through a gap on one field", SquadThroughGapTests},
        {"stage9 shared field and open ground", SharedFieldAndOpenFieldTests},
        {"stage9 squad machine, teams, losses", SquadMachineAndTeamsTests},
        {"stage9 flow field window and widening", WindowedFieldTests},
        {"stage10 squad at 60 FPS: re-planning in turns", StaggeredReplanTests},
        {"stage11 machines by name, AiCharacter property", MachineRegistryTests},
        {"stage12 smart objects: claim, use, release", SmartObjectTests},
        {"stage3 turret focus and the player", TurretAndPlayerTests},
    };
    const char* filter = argc > 1 ? argv[1] : "";
    int failures = 0, run = 0;
    for (const auto& test : tests)
    {
        if (*filter && !std::strstr(test.first, filter)) continue;
        ++run;
        try { test.second(); std::cout << "PASS: " << test.first << '\n'; }
        catch (const std::exception& error) { ++failures; std::cout << "FAIL: " << test.first << ": " << error.what() << '\n'; }
    }
    std::cout << run - failures << '/' << run << " AI tests passed\n";
    return failures ? 1 : 0;
}
