#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "inventory.h"
#include "../world/chunk_manager.h"

class Player {
public:
    Vector3 position;
    Camera3D camera;
    Inventory inventory;

    // physics settings
    float gravity;
    float jumpForce;
    float moveSpeed;
    float flySpeed;
    bool isFlying;
    float verticalVelocity;
    
    // camera angles
    float cameraAngleX;
    float cameraAngleY;
    Vector3 forward;
    Vector3 right;

    // editing state
    bool isBlockSelected;
    Vector3 selectedBlockPos;
    Vector3 selectedNormal;

    void Init();
    void Update(float dt, ChunkManager& world);
    void UpdateRaycast(ChunkManager& world);
    void HandleInput(ChunkManager& world); // handling breaking/placing
    
    int GetHeldBlockID() {
        return inventory.slots[inventory.selectedSlot].blockID;
    }
};

#endif