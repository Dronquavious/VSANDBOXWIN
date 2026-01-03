#include "game.h"
#include "../world/world_generator.h"
#include <fstream>
#include <cstring>

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
    // Open file in Binary Mode
    std::ofstream out("worlds/savegame.vxl", std::ios::binary);
    if (!out) {
        messageText = "FAILED TO SAVE GAME";
        messageTimer = 3.0f;
        return;
    }

    // HEADER (Magic Number + Version)
    const char* magic = "VOXL";
    int version = 1;
    out.write(magic, 4);
    out.write((char*)&version, sizeof(int));

    // WORLD GLOBAL DATA
    out.write((char*)&WorldGenerator::worldSeed, sizeof(int));

    // PLAYER DATA (Position, Camera, Inventory)
    // Since Inventory is a simple struct (POD), we can write it directly
    out.write((char*)&player.position, sizeof(Vector3));
    out.write((char*)&player.cameraAngleX, sizeof(float));
    out.write((char*)&player.cameraAngleY, sizeof(float));
    out.write((char*)&player.inventory, sizeof(Inventory));

    // CHUNK DATA
    world.SaveChunks(out);

    out.close();
    messageText = "GAME SAVED!";
    messageTimer = 2.0f;
}

void Game::LoadMap() {
    std::ifstream in("worlds/savegame.vxl", std::ios::binary);
    if (!in) {
        messageText = "NO SAVE FILE FOUND";
        messageTimer = 3.0f;
        return;
    }

    // HEADER CHECK
    char magic[5] = { 0 };
    in.read(magic, 4);
    if (strcmp(magic, "VOXL") != 0) {
        messageText = "INVALID SAVE FILE";
        messageTimer = 3.0f;
        return;
    }

    int version;
    in.read((char*)&version, sizeof(int));

    // WORLD GLOBAL DATA
    in.read((char*)&WorldGenerator::worldSeed, sizeof(int));

    // PLAYER DATA
    in.read((char*)&player.position, sizeof(Vector3));
    in.read((char*)&player.cameraAngleX, sizeof(float));
    in.read((char*)&player.cameraAngleY, sizeof(float));
    in.read((char*)&player.inventory, sizeof(Inventory));

    // recalculate player vectors so you don't snap incorrectly
    player.forward = { sinf(player.cameraAngleX), 0.0f, cosf(player.cameraAngleX) };
    player.right = { cosf(player.cameraAngleX), 0.0f, -sinf(player.cameraAngleX) };

    // CHUNK DATA
    world.LoadChunks(in);

    in.close();
    messageText = "GAME LOADED!";
    messageTimer = 2.0f;
}