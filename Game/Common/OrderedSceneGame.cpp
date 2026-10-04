#include "OrderedSceneGame.h"
#include <Engine/Components/GameComponents.h>

void OrderedSceneGame::Draw()
{
    BeginMainPass(false);

    for (size_t i = 0; i < Components.size(); ++i)
    {
        GameComponent* Component = FindComponent(std::to_string(i + 1));
        if (Component != nullptr && BindComponentShaders(Component, ShaderCompileVariant::Default))
        {
            Component->Render(Context.Get());
        }
    }
}
