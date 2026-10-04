#include "ExampleStartup.h"
#include "OrderedSceneGame.h"
#include "Scenes.h"
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace
{
enum class Scene { Choose, Triangles, Cube };

Scene ChooseScene()
{
    const TASKDIALOG_BUTTON buttons[] = {{100, L"Квадраты"}, {101, L"Куб"}};
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof(dialog);
    dialog.pszWindowTitle = L"BasicExamples";
    dialog.pszMainInstruction = L"Выберите сцену первой лабораторной";
    dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    dialog.cButtons = 2;
    dialog.pButtons = buttons;
    dialog.nDefaultButton = 100;
    int button = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&dialog, &button, nullptr, nullptr)))
        throw std::runtime_error("Cannot open scene selector");
    if (button == 100) return Scene::Triangles;
    if (button == 101) return Scene::Cube;
    return Scene::Choose; // Cancel closes the application without creating a game.
}
}

int main(int argc, char** argv)
{
    bool smoke = false;
    // Keep unattended diagnostics free of error dialogs even with invalid arguments.
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--smoke") smoke = true;
    try
    {
        Scene scene = Scene::Choose;
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--smoke") continue;
            if (argument == "--scene" && i + 1 < argc)
            {
                const std::string name = argv[++i];
                if (name == "triangles") { scene = Scene::Triangles; continue; }
                if (name == "cube") { scene = Scene::Cube; continue; }
            }
            throw std::runtime_error("Usage: BasicExamples.exe [--scene triangles|cube] [--smoke]");
        }
        if (scene == Scene::Choose) scene = smoke ? Scene::Triangles : ChooseScene();
        if (scene == Scene::Choose) return 0;
        ExampleStartup::PrepareContent();
        ExampleStartup::SmokeGame<OrderedSceneGame> game(smoke);
        if (!game.Initialize()) throw std::runtime_error("BasicExamples initialization failed");
        if (scene == Scene::Triangles) BuildTriangles(game);
        else BuildCube(game);
        const int result = game.StartGame();
        if (smoke && result == 0)
            std::cout << "BasicExamples/" << (scene == Scene::Triangles ? "triangles" : "cube") << ": 30 frames, normal shutdown\n";
        return result;
    }
    catch (const std::exception& error) { return ExampleStartup::ReportError(error, smoke); }
}
