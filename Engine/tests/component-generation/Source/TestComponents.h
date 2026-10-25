#pragma once
#include <Engine/Components/GameComponents.h>

namespace GenerationTest
{
    // ENGINE_COMPONENT("Not real") class Fake : public GameComponent {};
    inline const char* ignored = R"text(ENGINE_COMPONENT() class Fake {})text";

    ENGINE_COMPONENT("Moving object")
    class Mover final : public GameComponent
    {
    public:
        int ticks = 0;
        void Tick(float) override { ++ticks; }
    };

    class Intermediate : public GameComponent {};
    ENGINE_COMPONENT()
    class Indirect : public Intermediate {};
}
