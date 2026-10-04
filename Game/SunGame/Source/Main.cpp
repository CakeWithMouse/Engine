#include "SunGame.h"
#include "ExampleStartup.h"

int main(int argc, char** argv)
{
    bool smoke = false;
    try
    {
        smoke = ExampleStartup::ParseSmoke(argc, argv);
        const auto content = ExampleStartup::PrepareContent();
        SunGameConfig config;
        SunGame game(config, smoke ? 30 : 0);
        if (!game.Initialize()) throw std::runtime_error("SunGame initialization failed");
        game.BuildScene(content);
        const int result = game.StartGame();
        if (smoke && result == 0) std::cout << "SunGame: 30 frames, normal shutdown\n";
        return result;
    }
    catch (const std::exception& error) { return ExampleStartup::ReportError(error, smoke); }
}
