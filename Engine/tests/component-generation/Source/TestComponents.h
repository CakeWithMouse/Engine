#pragma once
#include <Engine/Components/GameComponents.h>
#include <Engine/Components/PropertyMacros.h>

namespace GenerationTest
{
    // ENGINE_COMPONENT("Not real") class Fake : public GameComponent {};
    inline const char* ignored = R"text(ENGINE_COMPONENT() class Fake {})text";

    ENGINE_COMPONENT("Moving object")
    class Mover : public GameComponent
    {
    public:
        int ticks = 0;
        ENGINE_GENERATED_PROPERTIES(GameComponent)
        ENGINE_PROPERTY("Speed",0,100)
        float speed=3;
        ENGINE_PROPERTY("Data",0,10)
        int data=1;
        void Tick(float) override { ++ticks; }
        int Mode() const { return mode; }
        const std::string& Title() const { return title; }
    private:
        ENGINE_ENUM_PROPERTY("Mode", {0,"Normal"}, {1,"Reverse"})
        int mode = 0;
        ENGINE_PROPERTY("Title")
        std::string title{"default"};
    };

    ENGINE_COMPONENT()
    class Inherited final : public Mover {
    public:
        ENGINE_GENERATED_PROPERTIES(Mover)
        ENGINE_PROPERTY("Extra", -2.5, 10.5)
        double extra = 1.5;
    };
    class Intermediate : public GameComponent {};
    ENGINE_COMPONENT()
    class Indirect : public Intermediate {};
}
