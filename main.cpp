#include "raylib.h"
#include "game.h"

int main()
{
    InitWindow(1280, 720, "VOXEL SANDBOX");
    SetTargetFPS(500);

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