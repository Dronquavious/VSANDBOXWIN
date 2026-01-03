#include "raylib.h"
#include "core/game.h"
#include "world/world_generator.h"
#include "core/constants.h"
#include <ctime>

// global var
int RENDER_DISTANCE = 4;

int main() {
    InitWindow(1600, 900, "VOXEL SANDBOX");
    SetTargetFPS(500);

    WorldGenerator::worldSeed = GetRandomValue(0, 1000000);
    Game game;
    game.Init();

    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }

    game.ShutDown();
    CloseWindow();
    return 0;
}