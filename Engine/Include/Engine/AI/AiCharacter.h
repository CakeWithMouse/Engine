#pragma once
#include <Engine/AI/World.h>
#include <Engine/Components/Character.h>
#include <Engine/Components/PropertyMacros.h>
#include <memory>
#include <string>

/**
 * Character driven by AI. BeginPlay creates its controller and calls SetupAi; subclasses
 * describe senses and run a state machine there. Without a subclass, the "stateMachine" property
 * names a machine registered in C++ (Ai::RegisterMachine, ENGINE_AI_MACHINE); it runs when SetupAi
 * has started none. A character with "Игрок" checked is never driven by AI.
 */
class AiCharacter : public Character
{
public:
    std::string stateMachine;        // name given to Ai::RegisterMachine, for example "Guard"; empty: SetupAi decides
    int team = 0;
    float sightRadius = 0, fieldOfView = 120, hearingRadius = 0;

    ENGINE_PROPERTIES(Character,
        ENGINE_PROPERTY_FIELD(stateMachine, "Автомат (имя из C++)")
        ENGINE_PROPERTY_FIELD(team, "Команда", 0, 255)
        ENGINE_PROPERTY_FIELD(sightRadius, "Радиус зрения (0 — не видит)", 0, 10000)
        ENGINE_PROPERTY_FIELD(fieldOfView, "Угол обзора, градусов", 1, 360)
        ENGINE_PROPERTY_FIELD(hearingRadius, "Радиус слуха (0 — не слышит)", 0, 10000)
    )

    void BeginPlay() override;
    /** The controller of this character; null before Play or for the player. */
    Ai::Controller* GetAi() const;

protected:
    /** Override to use your own controller class, like Game::CreateCharacterController. */
    virtual std::unique_ptr<Ai::Controller> CreateController(Ai::World& world);
    /** Senses, route and the state machine. Called once, after the properties are applied. */
    virtual void SetupAi(Ai::Controller&) {}
};

/**
 * Box that shapes navigation, like NavMeshBoundsVolume and NavModifierVolume in Unreal.
 * The box is the object's position and scale (a unit cube). Read once when the scene starts.
 */
class NavVolume : public GameComponent
{
public:
    int mode = 1;          // 0 bounds, 1 obstacle, 2 cost
    int cost = 5;          // for mode 2: 1 normal .. 254 expensive
    float cellSize = 1;    // for mode 0: size of a grid cell, metres

    ENGINE_PROPERTIES(GameComponent,
        ENGINE_ENUM_PROPERTY_FIELD(mode, "Режим", {0, "Границы навигации"}, {1, "Препятствие"}, {2, "Стоимость прохода"})
        ENGINE_PROPERTY_FIELD(cost, "Стоимость (1 — обычная)", 1, 254)
        ENGINE_PROPERTY_FIELD(cellSize, "Клетка сетки, м", 0.1, 100)
    )
    bool IsRenderable() const override { return false; }
    bool CastsShadow() const override { return false; }
    /** XZ rectangle of the box in world space. */
    void GetArea(glm::vec2& minimum, glm::vec2& maximum);
};
