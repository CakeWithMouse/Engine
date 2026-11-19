#pragma once

#include "GraphDocument.h"

namespace EditorCore
{

class FsmGraphPresets
{
public:
    static GraphDocument BuildIdlePatrol();
    static GraphDocument BuildIdleChase();
    static GraphDocument BuildIdleAttack();
};

} // namespace EditorCore
