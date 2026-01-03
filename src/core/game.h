#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "../world/chunk_manager.h"
#include "../player/player.h"
#include "../graphics/renderer.h"

enum GameState {
    STATE_MENU,
    STATE_LOADING,
    STATE_PLAYING
};

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
    bool LoadMap();
    
    // game state
    GameState currentState;

    // loading State
    int loadingProgress;    // 0 to 100
    bool isNewGame;         // are we generating or loading?

    // Auto-Save
    float autoSaveTimer;

    // internal Helpers
    void UpdateMenu();
    void DrawMenu();

    void UpdateLoading(); // function that loads chunks 1-by-1
    void DrawLoading();
};

#endif