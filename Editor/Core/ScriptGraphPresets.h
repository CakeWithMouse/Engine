#pragma once

#include "GraphDocument.h"

namespace EditorCore
{

class ScriptGraphPresets
{
public:
    static GraphDocument BuildEmpty();
    static GraphDocument BuildStartPrint();
    static GraphDocument BuildTickRotate();
};

} // namespace EditorCore
