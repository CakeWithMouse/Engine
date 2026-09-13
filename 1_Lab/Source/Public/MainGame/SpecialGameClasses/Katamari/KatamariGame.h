#pragma once
#include "../../BaseGameClass/Game.h"

class KatamariGame : public Game
{
public:
    KatamariGame(){};
    virtual void AfterInitialize() override;
    //Player* GetPlayer(){return FirstPlayer;}

protected:
    virtual void Draw(ID3D11RasterizerState* RasterState) override;
    virtual void DrawForward(ID3D11RasterizerState* RasterState) override;
    virtual void DrawDeffered(ID3D11RasterizerState* RasterState) override;
    std::vector<GameComponent*> HasCollision;
};
