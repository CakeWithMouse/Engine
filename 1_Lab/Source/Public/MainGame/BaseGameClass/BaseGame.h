#pragma once
#include "Game.h"

class BaseGame : public Game
{
public:
    BaseGame() :Game(){};
protected:
    virtual void Draw(ID3D11RasterizerState* RasterState) override;
};
