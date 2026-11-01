#include "Scenes.h"
#include "ExampleStartup.h"
#include <Engine/Components/SpecificComponents/TriangleComponent.h>

void BuildTriangles(Game& game)
{
    TriangleComponent* square1 = new TriangleComponent();
    square1->SetPosition({0, 0, 0.5f});
    square1->SetRotationSpeed(1.0f);
    ExampleStartup::Register(game, "1", square1);

    TriangleComponent* square2 = new TriangleComponent();
    square2->SetPosition({0, 0, 0.5f});
    square2->SetScale({0.5f, 0.5f, 1.0f});
    square2->SetRotationSpeed(-1.5f);
    ExampleStartup::Register(game, "2", square2);

}
