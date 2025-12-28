#include "raylib.h"
#include "game.h"
#include <ctime>

int main()
{
    InitWindow(1280, 720, "VOXEL SANDBOX");
    SetTargetFPS(500);

    SetRandomSeed((unsigned int)time(0));
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