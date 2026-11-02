#include "Scenes.h"
#include "ExampleStartup.h"
#include <Engine/Components/SpecificComponents/CubeComponent.h>

void BuildCube(Game& game)
{
    CubeComponent* Cube = new CubeComponent();

    ExampleStartup::Register(game, "1", Cube);

}
