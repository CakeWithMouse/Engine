#pragma once
#include <Engine/Components/GameComponents.h>

ENGINE_COMPONENT("Example component")
class ExampleComponent : public GameComponent
{
public:
    ExampleComponent() = default;
};
