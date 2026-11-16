#include <Engine/MainGame/BaseGameClass/Game.h>
// These examples are compiled with EngineTests and use no game-specific rules.
Trace::Result WorldTrace(Scene &scene)
{
    Trace::Query query;
    query.maxDistance = 100;
    return scene.Raycast({{0, 10, 0}, {0, -1, 0}, 100}, query);
}
Trace::Result PixelTrace(Game &game, uint32_t x, uint32_t y)
{
    return game.RaycastScreen(Trace::ViewportPoint::Pixel(x, y));
}
Trace::Result CursorTrace(Game &game)
{
    return game.RaycastCursor(); // InputBlocked when UI captures the pointer or Game input is disabled.
}
Trace::RayResult ScreenRay(Game &game)
{
    return game.ScreenPointToRay({120.25f, 80.75f}); // physical viewport pixels; no rounding
}
