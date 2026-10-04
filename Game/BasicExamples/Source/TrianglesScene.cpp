#include "Scenes.h"
#include "ExampleStartup.h"
#include <Engine/Components/SpecificComponents/TriangleComponent.h>
#include "SquareGeometry.h"

void BuildTriangles(Game& game)
{
    TriangleComponent* square1 = new TriangleComponent();
    square1->InitSquare(Square::SquarePoints[0], Square::SquareIndices);
    square1->SetRotationSpeed(1.0f);
    ExampleStartup::Register(game, "1", square1);

    TriangleComponent* square2 = new TriangleComponent();
    square2->InitSquare(Square::SquarePoints[1], Square::SquareIndices);
    square2->SetRotationSpeed(-1.5f);
    ExampleStartup::Register(game, "2", square2);

}
