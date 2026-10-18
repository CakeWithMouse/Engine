#include "BattleGame.h"
#include "ExampleStartup.h"

#include <iostream>
#include <string>

namespace
{
int ParseInt(const std::string& value, const char* option)
{
    try
    {
        return std::stoi(value);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error(std::string("Expected a number after ") + option);
    }
}
}

int main(int argc, char** argv)
{
    bool smoke = false;
    bool bench = false;
    uint64_t benchFrames = 600;
    uint64_t warmupFrames = 150;
    BattleGame::Settings settings;
    uint64_t frameLimit = 0;

    try
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--smoke") { smoke = true; continue; }
            if (argument == "--bench")
            {
                bench = true;
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    benchFrames = static_cast<uint64_t>(ParseInt(argv[++i], "--bench"));
                }
                continue;
            }
            if (argument == "--warmup" && i + 1 < argc) { warmupFrames = static_cast<uint64_t>(ParseInt(argv[++i], "--warmup")); continue; }
            if (argument == "--units" && i + 1 < argc) { settings.UnitCount = ParseInt(argv[++i], "--units"); continue; }
            if (argument == "--spacing" && i + 1 < argc) { settings.Spacing = static_cast<float>(ParseInt(argv[++i], "--spacing")); continue; }
            if (argument == "--camera" && i + 1 < argc) { settings.CameraRadius = static_cast<float>(ParseInt(argv[++i], "--camera")); continue; }
            if (argument == "--pitch" && i + 1 < argc) { settings.CameraPitch = static_cast<float>(ParseInt(argv[++i], "--pitch")); continue; }
            if (argument == "--frames" && i + 1 < argc) { frameLimit = static_cast<uint64_t>(ParseInt(argv[++i], "--frames")); continue; }
            if (argument == "--no-shadows") { settings.Shadows = false; continue; }
            if (argument == "--forward") { settings.Forward = true; continue; }
            if (argument == "--static") { settings.Moving = false; continue; }
            if (argument == "--no-impostors") { settings.Impostors = false; continue; }
            if (argument == "--detail" && i + 1 < argc) { settings.Detail = ParseInt(argv[++i], "--detail"); continue; }
            if (argument == "--dump-shadows") { settings.DumpShadows = true; continue; }
            if (argument == "--sun" && i + 1 < argc) { settings.SunElevation = static_cast<float>(ParseInt(argv[++i], "--sun")); continue; }
            if (argument == "--scenery" && i + 1 < argc) { settings.SceneryCount = ParseInt(argv[++i], "--scenery"); continue; }
            if (argument == "--types" && i + 1 < argc) { settings.VehicleTypes = ParseInt(argv[++i], "--types"); continue; }
            if (argument == "--no-baked-shadows") { settings.BakedShadows = false; continue; }
            if (argument == "--sprite-px" && i + 1 < argc) { settings.SpritePixels = static_cast<float>(ParseInt(argv[++i], "--sprite-px")); continue; }
            if (argument == "--lod-debug") { settings.LodDebugColors = true; continue; }
            throw std::runtime_error(
                "Usage: Battle.exe [--bench N] [--warmup N] [--units N] [--types N] [--spacing N] [--camera N] [--frames N] [--detail N]\n"
                "        [--scenery N] [--sprite-px N] [--sun N] [--no-shadows] [--no-baked-shadows]\n"
                "        [--no-impostors] [--forward] [--static] [--lod-debug] [--dump-shadows] [--smoke]");
        }
        if (settings.UnitCount < 1) throw std::runtime_error("--units must be positive");
        if (smoke && frameLimit == 0) frameLimit = 30;

        ExampleStartup::PrepareContent();
        BattleGame game;
        if (!game.Initialize()) throw std::runtime_error("Battle initialization failed");

        if (bench)
        {
            game.SetVSync(false);
            if (game.IsVSyncActive())
            {
                std::cerr << "Error: VSync is enabled, benchmark invalid" << std::endl;
                return 1;
            }
            game.EnableBench(benchFrames, warmupFrames);
        }
        else
        {
            game.SetFrameLimit(frameLimit);
            // Unattended runs measure the renderer, not the display refresh rate.
            if (frameLimit > 0) game.SetVSync(false);
        }

        game.BuildScene(settings);

        if (bench)
        {
            game.PrintBenchHeader();
        }

        const int result = game.StartGame();
        if (result != 0)
        {
            return result;
        }

        if (bench)
        {
            std::cout << game.BuildBenchReport() << std::endl;
        }
        else
        {
            // One result line per run, so several runs can be compared directly.
            std::cout << "battle-measurement " << game.BuildReport() << std::endl;
        }
        return result;
    }
    catch (const std::exception& error)
    {
        return ExampleStartup::ReportError(error, smoke);
    }
}
