#pragma once
#include "../../BaseGameClass/Game.h"

class SunGame : public Game
{
public:
    SunGame() : Game(){};

protected:
    virtual void Draw(ID3D11RasterizerState* RasterState) override;
    virtual void DrawForward(ID3D11RasterizerState* RasterState) override;
    virtual void DrawDeffered(ID3D11RasterizerState* RasterState) override;
};
