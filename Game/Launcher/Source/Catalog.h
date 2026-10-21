#pragma once
#include <array>

struct GameEntry
{
    const wchar_t* directory;
    const wchar_t* title;
    const wchar_t* buildHint;
    bool runtime = false;
    bool preview = false;
    bool requiresAssimp = false;
    const wchar_t* extraArgs = L"";
};

// Game names belong to the launcher, never to Engine.lib.
inline constexpr std::array<GameEntry, 7> Games = {{
    {L"RuntimeExample", L"Runtime Example — редактор с BasicExamples preview", L"Запустите Start.cmd для сборки.", true, true},
    {L"BasicExamples", L"BasicExamples — квадраты / куб", L"Запустите Start.cmd для сборки.", false, true},
    {L"Pong", L"Pong — ракетки и мяч", L"Запустите Start.cmd для сборки.", false, true},
    {L"SunGame", L"SunGame — солнечная система", L"Запустите Start.cmd для сборки.", false, true},
    {L"Battle", L"Battle — массовая техника, LOD и инстансинг", L"Запустите Start.cmd для сборки.", false, true},
    {L"Battle", L"Battle (Animated Crowd) — анимированная толпа 10 000+ скелетов (VAT, Assimp)", L"Запустите Start.cmd для сборки с Assimp.", false, false, true, L"--anim"},
    {L"Katamari", L"Katamari — сбор объектов (требуется Assimp)", L"Нужен установленный vcpkg с Assimp. Задайте VCPKG_ROOT и запустите Start-With-Assimp.cmd.", false, false, true}
}};
