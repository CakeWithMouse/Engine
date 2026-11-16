#pragma once
#include <Engine/MainGame/BaseGameClass/Game.h>
#include <filesystem>
#include <string>

/**
 * Visual scripts (.vscript) on procedural meshes, no C++ gameplay code: a cube turns (Spin), a sphere
 * floats (Bob) and a cube moves with the Left/Right arrows and returns on Space (Move). The scripts are
 * in GameContent/Scripts; edit them (by hand or in the editor's script window) and restart the game.
 * Scene time runs at real speed here, as in editor Play, so the scripts' "per second" values hold.
 */
class ScriptsGame : public Game
{
public:
    /** Call after Initialize. Throws if a script file is missing. */
    void BuildScene(const std::filesystem::path& gameContent);
    /** After the game ran: empty if every script is running and the spinner turned, otherwise why not. */
    std::string Verify() const;

protected:
    std::array<float, 4> GetClearColor() const override { return {0.08f, 0.09f, 0.12f, 1.0f}; }
};
