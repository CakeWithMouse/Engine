#pragma once

#include "Graph/GraphDocument.h"

namespace EditorCore
{

class FsmGraphPresets
{
public:
    static GraphDocument BuildIdlePatrol();
    static GraphDocument BuildIdleChase();
    static GraphDocument BuildIdleAttack();
    static GraphDocument BuildPatrolChaseReturn();
    static GraphDocument BuildGuardAttackDistance();
    static GraphDocument BuildIdleFlee();
};

} // namespace EditorCore
