#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "chunk_manager.h"
#include <fstream>

// Constants
#define CHUNK_SIZE 32
#define BLOCK_TEX_SIZE 16

struct InventoryItem {
    int blockID; // 0 = Empty
    int count;   // for survival mode later
};

struct Inventory {
    InventoryItem slots[36]; // 9 Hotbar slots (0-8), 27 Storage (9-35)
    int selectedSlot;        // 0-8 (Which hotbar slot is active)
};

class Game
{
public:
    void Init();
    void update();
    void draw();
    void shutDown();
    void DrawDebug();

private:
    // --- SUB-SYSTEM FUNCTIONS ---
    void UpdatePlayer();
    void UpdateRaycast();
    void EditMap();

    void DrawWorld(Color tint);
    void DrawHand();
    void DrawUI();

    void SaveMap();
    void LoadMap();

    // Helper for collision
    bool IsPositionSolid(Vector3 pos);

private:
    // --- MEMBER VARIABLES ---

    // Core Systems
    Camera3D camera;
    ChunkManager world;
    Model blockModel;

    // Resources (Assets)
    Texture2D texDirt, texStone, texWood, texGrass, texGrassSide;
    Texture2D texSand, texBedrock, texLeaves, texSnow, texCactus;

    // Texture Generators
    Texture2D GenStoneTexture(int size);
    Texture2D GenDirtTexture(int size);
    Texture2D GenGrassTexture(int size);
    Texture2D GenWoodTexture(int size);
    Texture2D GenSandTexture(int size);
    Texture2D GenBedrockTexture(int size);
    Texture2D GenLeavesTexture(int size);
    Texture2D GenGrassSideTexture(int size);
    Texture2D GenSnowTexture(int size);   
    Texture2D GenCactusTexture(int size);

    // Game State
    Inventory inventory;
    int GetHeldBlockID() {
        return inventory.slots[inventory.selectedSlot].blockID;
    }
    bool isBlockSelected;
    Vector3 selectedBlockPos;
    Vector3 selectedNormal;

    // Environment
    float timeOfDay; // 0.0 to 1.0
    int timeMode;

    // Player Physics & Input
    Vector3 playerPosition;
    float cameraAngleX;
    float cameraAngleY;
    Vector3 forward;
    Vector3 right;

    bool isFlying;
    float verticalVelocity;
    float handBobbing;

    // Debug & Settings
    bool showDebugUI;
    float gravity;
    float jumpForce;
    float moveSpeed;
    float flySpeed;
    float daySpeed;

    // GUI Notifications
    float messageTimer;
    const char *messageText;

    bool IsBlockActive(int x, int y, int z); // Helper for culling
};

#endif