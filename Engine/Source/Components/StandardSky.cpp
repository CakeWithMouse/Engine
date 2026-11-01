#include <Engine/Project/StandardTypes.h>
#include "ComponentInternal.h"

SceneProject::Sky::Sky()
{
    const auto shader = EnginePaths::Shader("BasicSky.hlsl").u8string();
    EngineInternal::ComponentAccess::Shaders(*this, shader, shader);
}
