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
};

} // namespace EditorCore
