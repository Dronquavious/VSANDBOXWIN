#include "raylib.h"
#include "game.h"
#include "world_gen.h"
#include <ctime>

int main()
{
    InitWindow(1280, 720, "VOXEL SANDBOX");
    SetTargetFPS(500);

    WorldGen::worldSeed = GetRandomValue(0, 1000000);
    Game game;

    game.Init();
    while (!WindowShouldClose())
    {
        game.update();
        game.draw();
    }

    CloseWindow();
    return 0;
}