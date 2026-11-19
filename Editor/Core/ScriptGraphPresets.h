#pragma once

#include "Graph/GraphDocument.h"

namespace EditorCore
{

class ScriptGraphPresets
{
public:
    static GraphDocument BuildEmpty();
    static GraphDocument BuildStartPrint();
    static GraphDocument BuildTickRotate();
    static GraphDocument BuildSinMoveY();
    static GraphDocument BuildTimerAction();
    static GraphDocument BuildCounterVariable();
    static GraphDocument BuildConditionThreshold();
};

} // namespace EditorCore
