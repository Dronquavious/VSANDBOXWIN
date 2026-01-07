#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "inventory.h"
#include "../world/chunk_manager.h"

/**
 * controls the first person camera and player physics
 * handles movement, collision, and world interaction
 */
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
    
    // camera orientation
    float cameraAngleX;
    float cameraAngleY;
    Vector3 forward;
    Vector3 right;

    // interaction state
    bool isBlockSelected;
    Vector3 selectedBlockPos;
    Vector3 selectedNormal;

    void Init();

    /**
     * updates physics and movement logic
     */
    void Update(float dt, ChunkManager& world);

    /**
     * performs raycasting for block selection
     */
    void UpdateRaycast(ChunkManager& world);

    /**
     * handles block breaking and placement input
     */
    void HandleInput(ChunkManager& world); 
    
    int GetHeldBlockID() {
        return inventory.slots[inventory.selectedSlot].blockID;
    }
};

#endif