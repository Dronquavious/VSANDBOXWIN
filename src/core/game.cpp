#include "game.h"
#include "../world/world_generator.h"

void Game::Init() {
    // defaults
    daySpeed = 0.005f;
    timeOfDay = 0.0f;
    timeMode = 0;
    showDebugUI = false;
    messageTimer = 0.0f;
    messageText = "";
    physicsTimer = 0.0f;

    DisableCursor();
    HideCursor();

    world.Init();
    player.Init();
    renderer.Init();
}

void Game::Update() {

    physicsTimer += GetFrameTime();
    if (physicsTimer >= 0.05f) {
        physicsTimer -= 0.05f;
        world.UpdateChunkPhysics();
    }

    // time of day logic
    if (timeMode == 0) { // cycle
        timeOfDay += GetFrameTime() * daySpeed;
        if (timeOfDay > 1.0f) timeOfDay = 0.0f;
    } else if (timeMode == 1) { // always day
        timeOfDay = 0.5f; 
    } else if (timeMode == 2) { // always night
        timeOfDay = 0.0f; 
    }

    if (messageTimer > 0.0f) messageTimer -= GetFrameTime();

    if (IsKeyPressed(KEY_TAB)) {
        showDebugUI = !showDebugUI;
        if (showDebugUI) EnableCursor();
        else DisableCursor();
    }

    if (!showDebugUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        DisableCursor();
    }

    if (showDebugUI) {
        // menu mode
    } else {
        if (IsKeyPressed(KEY_P)) SaveMap();
        if (IsKeyPressed(KEY_L)) LoadMap();

        player.Update(GetFrameTime(), world);
        player.UpdateRaycast(world);
        player.HandleInput(world);
    }
}

void Game::Draw() {
    renderer.DrawScene(player, world, timeOfDay);
    renderer.DrawUI(player, GetScreenWidth(), GetScreenHeight(), messageText, messageTimer);
    if (showDebugUI) {
        renderer.DrawDebug(player, daySpeed, timeMode);
    }
    EndDrawing();
}

void Game::ShutDown() {
    renderer.Unload();
    world.UnloadAll();
}

void Game::SaveMap() {
    messageText = "SAVING DISABLED (INFINITE WORLD)";
    messageTimer = 2.0f;
}

void Game::LoadMap() {
    messageText = "LOADING DISABLED (INFINITE WORLD)";
    messageTimer = 2.0f;
}