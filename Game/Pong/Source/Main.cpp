#include "ExampleStartup.h"
#include "PongGame.h"

int RunScene(bool smoke)
{
    auto MainGame = std::make_unique<ExampleStartup::SmokeGame<PongGame>>(smoke);
    if (!MainGame->Initialize())
    {
        return 1;
    }

    MainGame->BuildScene();
    return MainGame->StartGame();

}

int main(int argc, char** argv)
{
    bool smoke = false;
    try
    {
        smoke = ExampleStartup::ParseSmoke(argc, argv);
        ExampleStartup::PrepareContent();
        const int result = RunScene(smoke);
        if (smoke && result == 0) std::cout << "Pong: 30 frames, normal shutdown\n";
        if (result != 0) throw std::runtime_error("Game initialization or execution failed");
        return result;
    }
    catch (const std::exception& error) { return ExampleStartup::ReportError(error, smoke); }
}
