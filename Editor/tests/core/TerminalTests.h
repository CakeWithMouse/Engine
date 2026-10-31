#pragma once

#include "../../Core/Terminal.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{

void CheckTerm(bool cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL (Terminal): %s\n", msg);
        std::exit(1);
    }
}

inline void RunTerminalTests()
{
    using namespace EditorCore;

    // 1. Quotes tests
    {
        auto cmd = TerminalParser::Parse(QStringLiteral("select \"Player Character\""));
        CheckTerm(cmd.isValid, "select with double quotes is valid");
        CheckTerm(cmd.commandId == QStringLiteral("Scene.Select"), "commandId is Scene.Select");
        CheckTerm(cmd.parameters[QStringLiteral("name")].toString() == QStringLiteral("Player Character"), "Quoted name extracted intact");

        auto cmdEsc = TerminalParser::Parse(QStringLiteral("spawn \"RuntimeExample::MovingCube\" \"Main \\\"Hero\\\" Box\""));
        CheckTerm(cmdEsc.isValid, "spawn with escaped quotes is valid");
        CheckTerm(cmdEsc.parameters[QStringLiteral("type")].toString() == QStringLiteral("RuntimeExample::MovingCube"), "Type extracted intact");
        CheckTerm(cmdEsc.parameters[QStringLiteral("name")].toString() == QStringLiteral("Main \"Hero\" Box"), "Escaped quote preserved inside name");

        auto cmdSingle = TerminalParser::Parse(QStringLiteral("select 'Single Quoted Target'"));
        CheckTerm(cmdSingle.isValid, "select with single quotes is valid");
        CheckTerm(cmdSingle.parameters[QStringLiteral("name")].toString() == QStringLiteral("Single Quoted Target"), "Single quoted string extracted");
    }

    // 2. Count tests
    {
        auto cmdDup = TerminalParser::Parse(QStringLiteral("duplicate 5"));
        CheckTerm(cmdDup.isValid, "duplicate with count is valid");
        CheckTerm(cmdDup.commandId == QStringLiteral("Scene.Duplicate"), "commandId is Scene.Duplicate");
        CheckTerm(cmdDup.parameters[QStringLiteral("count")].toInt() == 5, "Count is 5 for duplicate");

        auto cmdSpawnCount = TerminalParser::Parse(QStringLiteral("spawn Cube 10"));
        CheckTerm(cmdSpawnCount.isValid, "spawn with positional count is valid");
        CheckTerm(cmdSpawnCount.parameters[QStringLiteral("count")].toInt() == 10, "Positional count is 10");

        auto cmdSpawnFlag = TerminalParser::Parse(QStringLiteral("spawn Cube \"MyCube\" -count 25"));
        CheckTerm(cmdSpawnFlag.isValid, "spawn with -count flag is valid");
        CheckTerm(cmdSpawnFlag.parameters[QStringLiteral("name")].toString() == QStringLiteral("MyCube"), "Name preserved with -count");
        CheckTerm(cmdSpawnFlag.parameters[QStringLiteral("count")].toInt() == 25, "Flag count is 25");
    }

    // 3. X, Y, Z coordinates tests
    {
        auto cmdPos = TerminalParser::Parse(QStringLiteral("pos 10.5 -2.5 30.0"));
        CheckTerm(cmdPos.isValid, "pos x y z is valid");
        CheckTerm(cmdPos.commandId == QStringLiteral("Scene.SetPosition"), "commandId is Scene.SetPosition");
        CheckTerm(std::fabs(cmdPos.parameters[QStringLiteral("x")].toDouble() - 10.5) < 1e-4, "Pos X matches 10.5");
        CheckTerm(std::fabs(cmdPos.parameters[QStringLiteral("y")].toDouble() - (-2.5)) < 1e-4, "Pos Y matches -2.5");
        CheckTerm(std::fabs(cmdPos.parameters[QStringLiteral("z")].toDouble() - 30.0) < 1e-4, "Pos Z matches 30.0");

        auto cmdPosX = TerminalParser::Parse(QStringLiteral("pos x 42.0"));
        CheckTerm(cmdPosX.isValid, "pos x is valid");
        CheckTerm(std::fabs(cmdPosX.parameters[QStringLiteral("x")].toDouble() - 42.0) < 1e-4, "Pos single X matches 42.0");

        auto cmdRot = TerminalParser::Parse(QStringLiteral("rot 0.0 180.0 90.0"));
        CheckTerm(cmdRot.isValid, "rot x y z is valid");
        CheckTerm(cmdRot.commandId == QStringLiteral("Scene.SetRotation"), "commandId is Scene.SetRotation");
        CheckTerm(std::fabs(cmdRot.parameters[QStringLiteral("y")].toDouble() - 180.0) < 1e-4, "Rot Y matches 180.0");

        auto cmdScaleUni = TerminalParser::Parse(QStringLiteral("scale 2.5"));
        CheckTerm(cmdScaleUni.isValid, "scale uniform is valid");
        CheckTerm(cmdScaleUni.commandId == QStringLiteral("Scene.SetScale"), "commandId is Scene.SetScale");
        CheckTerm(std::fabs(cmdScaleUni.parameters[QStringLiteral("x")].toDouble() - 2.5) < 1e-4, "Uniform scale X matches 2.5");
        CheckTerm(std::fabs(cmdScaleUni.parameters[QStringLiteral("y")].toDouble() - 2.5) < 1e-4, "Uniform scale Y matches 2.5");
        CheckTerm(std::fabs(cmdScaleUni.parameters[QStringLiteral("z")].toDouble() - 2.5) < 1e-4, "Uniform scale Z matches 2.5");

        auto cmdScale3 = TerminalParser::Parse(QStringLiteral("scale 1.0 2.0 3.0"));
        CheckTerm(cmdScale3.isValid, "scale 3 axes is valid");
        CheckTerm(std::fabs(cmdScale3.parameters[QStringLiteral("z")].toDouble() - 3.0) < 1e-4, "Scale Z matches 3.0");
    }

    // 4. Other commands
    {
        CheckTerm(TerminalParser::Parse("help").commandId == "App.Help", "help mapped");
        CheckTerm(TerminalParser::Parse("help spawn").parameters["topic"] == "spawn", "help topic mapped");
        CheckTerm(TerminalParser::Parse("deselect").commandId == "Scene.Deselect", "deselect mapped");
        CheckTerm(TerminalParser::Parse("delete").commandId == "Scene.Delete", "delete mapped");
        CheckTerm(TerminalParser::Parse("focus").commandId == "Viewport.Focus", "focus mapped");
        CheckTerm(TerminalParser::Parse("play").commandId == "Session.Play", "play mapped");
        CheckTerm(TerminalParser::Parse("pause").commandId == "Session.Pause", "pause mapped");
        CheckTerm(TerminalParser::Parse("stop").commandId == "Session.Stop", "stop mapped");
        CheckTerm(TerminalParser::Parse("clear").commandId == "Terminal.Clear", "clear mapped");
    }

    // 5. Error handling
    {
        auto cmdEmpty = TerminalParser::Parse("   ");
        CheckTerm(!cmdEmpty.isValid, "Empty line invalid");

        auto cmdUnknown = TerminalParser::Parse("foobar 123");
        CheckTerm(!cmdUnknown.isValid, "Unknown command invalid");
        CheckTerm(!cmdUnknown.errorMessage.isEmpty(), "Unknown command error message populated");

        auto cmdBadPos = TerminalParser::Parse("pos 1.0 abc");
        CheckTerm(!cmdBadPos.isValid, "Malformed pos invalid");
    }
}

} // namespace
