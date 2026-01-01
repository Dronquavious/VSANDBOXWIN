#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "../world/chunk_manager.h"
#include "../player/player.h"
#include "../graphics/renderer.h"

class Game {
public:
    void Init();
    void Update();
    void Draw();
    void ShutDown();

private:
    ChunkManager world;
    Player player;
    Renderer renderer;

    // environment settings
    float timeOfDay;
    int timeMode;
    float daySpeed;
    float physicsTimer;

    // debug & ui state
    bool showDebugUI;
    float messageTimer;
    const char* messageText;

    void SaveMap();
    void LoadMap();
};

#endif