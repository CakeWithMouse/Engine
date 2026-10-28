#include "ExampleStartup.h"
#include "MaterialsScene.h"
#include "OrderedSceneGame.h"
#include "Scenes.h"
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace
{
enum class ExampleScene { Choose, Triangles, Cube, Materials };

const char* SceneName(ExampleScene scene)
{
    return scene == ExampleScene::Triangles ? "triangles" : scene == ExampleScene::Cube ? "cube" : "materials";
}

ExampleScene ChooseScene()
{
    const TASKDIALOG_BUTTON buttons[] = {{100, L"Квадраты"}, {101, L"Куб"}, {102, L"Материалы"}};
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof(dialog);
    dialog.pszWindowTitle = L"BasicExamples";
    dialog.pszMainInstruction = L"Выберите сцену первой лабораторной";
    dialog.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    dialog.cButtons = 3;
    dialog.pButtons = buttons;
    dialog.nDefaultButton = 100;
    int button = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&dialog, &button, nullptr, nullptr)))
        throw std::runtime_error("Cannot open scene selector");
    if (button == 100) return ExampleScene::Triangles;
    if (button == 101) return ExampleScene::Cube;
    if (button == 102) return ExampleScene::Materials;
    return ExampleScene::Choose; // Cancel closes the application without creating a game.
}
}

int main(int argc, char** argv)
{
    bool smoke = false;
    // Keep unattended diagnostics free of error dialogs even with invalid arguments.
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--smoke") smoke = true;
    try
    {
        ExampleScene scene = ExampleScene::Choose;
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--smoke") continue;
            if (argument == "--scene" && i + 1 < argc)
            {
                const std::string name = argv[++i];
                if (name == "triangles") { scene = ExampleScene::Triangles; continue; }
                if (name == "cube") { scene = ExampleScene::Cube; continue; }
                if (name == "materials") { scene = ExampleScene::Materials; continue; }
            }
            throw std::runtime_error("Usage: BasicExamples.exe [--scene triangles|cube|materials] [--smoke]");
        }
        if (scene == ExampleScene::Choose) scene = smoke ? ExampleScene::Triangles : ChooseScene();
        if (scene == ExampleScene::Choose) return 0;
        const std::filesystem::path gameContent = ExampleStartup::PrepareContent();
        int result = 0;
        if (scene == ExampleScene::Materials)
        {
            ExampleStartup::SmokeGame<MaterialsGame> game(smoke);
            if (!game.Initialize()) throw std::runtime_error("BasicExamples initialization failed");
            game.BuildScene(gameContent);
            result = game.StartGame();
        }
        else
        {
            ExampleStartup::SmokeGame<OrderedSceneGame> game(smoke);
            if (!game.Initialize()) throw std::runtime_error("BasicExamples initialization failed");
            if (scene == ExampleScene::Triangles) BuildTriangles(game);
            else BuildCube(game);
            result = game.StartGame();
        }
        if (smoke && result == 0)
            std::cout << "BasicExamples/" << SceneName(scene) << ": 30 frames, normal shutdown\n";
        return result;
    }
    catch (const std::exception& error) { return ExampleStartup::ReportError(error, smoke); }
}
