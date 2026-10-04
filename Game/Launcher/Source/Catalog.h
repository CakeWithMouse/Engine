#pragma once
#include <array>

struct GameEntry
{
    const wchar_t* directory;
    const wchar_t* title;
    const wchar_t* buildHint;
    bool runtime = false;
};

// Game names belong to the launcher, never to Engine.lib.
inline constexpr std::array<GameEntry, 5> Games = {{
    {L"RuntimeExample", L"Runtime Example — игра / редактор (заглушки)", L"Запустите Start.cmd для сборки.", true},
    {L"BasicExamples", L"BasicExamples — квадраты / куб", L"Запустите Start.cmd для сборки."},
    {L"Pong", L"Pong — ракетки и мяч", L"Запустите Start.cmd для сборки."},
    {L"SunGame", L"SunGame — солнечная система", L"Запустите Start.cmd для сборки."},
    {L"Katamari", L"Katamari — сбор объектов (требуется Assimp)", L"Нужен установленный vcpkg с Assimp. Задайте VCPKG_ROOT и запустите Start-With-Assimp.cmd."}
}};
