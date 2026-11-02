#pragma once

#include "MaterialGraphModel.h"
#include <QString>

namespace EditorCore
{

class HlslGraphCompiler
{
public:
    static QString CompileGraphToHlsl(const MaterialGraphModel& model);
};

} // namespace EditorCore
