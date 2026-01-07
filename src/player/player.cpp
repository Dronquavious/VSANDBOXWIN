#include "player.h"
#include "raymath.h"
#include "../blocks/block_types.h"

/**
 * initializes player state and camera
 */
void Player::Init() {
    position = Vector3{ 16.0f, 100.0f, 16.0f };
    cameraAngleX = 0.0f;
    cameraAngleY = 0.0f;
    isFlying = true;
    verticalVelocity = 0.0f;

    // inventory setup
    inventory.selectedSlot = 0;
    inventory.slots[0] = { (int)BlockType::DIRT, 64 };
    inventory.slots[1] = { (int)BlockType::STONE, 64 };
    inventory.slots[2] = { (int)BlockType::WOOD, 64 };
    inventory.slots[3] = { (int)BlockType::GRASS, 64 };
    inventory.slots[4] = { (int)BlockType::SAND, 64 };
    inventory.slots[5] = { (int)BlockType::SNOW, 64 };
    inventory.slots[6] = { (int)BlockType::CACTUS, 64 };
    inventory.slots[7] = { (int)BlockType::TORCH, 64 };
    inventory.slots[8] = { (int)BlockType::GLOWSTONE, 64 };

    gravity = 0.015f;
    jumpForce = 0.25f;
    moveSpeed = 4.0f;
    flySpeed = 10.0f;

    camera.position = Vector3{ 0.0f, 10.0f, 10.0f };
    camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

/**
 * updates position, physics, and camera view
 */
void Player::Update(float dt, ChunkManager& world) {
    if (dt > 0.05f) dt = 0.05f; // cap timestep

    // mouse look
    Vector2 mouseDelta = GetMouseDelta();
    cameraAngleX -= mouseDelta.x * 0.003f;
    cameraAngleY -= mouseDelta.y * 0.003f;
    if (cameraAngleY > 1.5f) cameraAngleY = 1.5f;
    if (cameraAngleY < -1.5f) cameraAngleY = -1.5f;

    forward = { sinf(cameraAngleX), 0.0f, cosf(cameraAngleX) };
    right = { cosf(cameraAngleX), 0.0f, -sinf(cameraAngleX) };

    float baseSpeed = isFlying ? flySpeed : moveSpeed;
    if (IsKeyDown(KEY_LEFT_SHIFT)) baseSpeed *= 2.0f;
    float step = baseSpeed * dt;

    Vector3 moveVec = { 0,0,0 };
    if (IsKeyDown(KEY_W)) { moveVec.x += forward.x; moveVec.z += forward.z; }
    if (IsKeyDown(KEY_S)) { moveVec.x -= forward.x; moveVec.z -= forward.z; }
    if (IsKeyDown(KEY_D)) { moveVec.x -= right.x; moveVec.z -= right.z; }
    if (IsKeyDown(KEY_A)) { moveVec.x += right.x; moveVec.z += right.z; }

    if (Vector3Length(moveVec) > 0) {
        moveVec = Vector3Normalize(moveVec);
        moveVec = Vector3Scale(moveVec, step);
    }

    if (IsKeyPressed(KEY_F)) isFlying = !isFlying;

    // physics application
    if (isFlying) {
        position.x += moveVec.x;
        position.z += moveVec.z;
        if (IsKeyDown(KEY_SPACE)) position.y += step;
        if (IsKeyDown(KEY_LEFT_CONTROL)) position.y -= step;
        verticalVelocity = 0.0f;
    } else {
        float dtScale = dt * 150.0f;
        verticalVelocity -= gravity * dtScale;
        if (verticalVelocity < -0.5f) verticalVelocity = -0.5f;

        position.y += verticalVelocity * dtScale;

        // collision checks
        float playerHeight = 1.5f;
        // float playerWidth = 0.3f; // unused variables removed

        if (verticalVelocity <= 0.0f) {
            bool onGround = false;
            float lowestY = position.y - playerHeight;

            for (int dx = 0; dx <= 1; dx++) {
                for (int dz = 0; dz <= 1; dz++) {
                    int checkX = (int)floor(position.x + (dx * 0.5f));
                    int checkZ = (int)floor(position.z + (dz * 0.5f));
                    int checkY = (int)floor(lowestY - 0.1f);

                    if (world.GetBlock(checkX, checkY, checkZ) != BlockType::AIR) {
                        float blockTop = (float)checkY + 1.0f;
                        if (lowestY - 0.1f < blockTop) {
                            position.y = blockTop + playerHeight;
                            onGround = true;
                        }
                    }
                }
            }

            if (onGround) {
                verticalVelocity = 0.0f;
                if (IsKeyDown(KEY_SPACE)) verticalVelocity = jumpForce;
            }
        }

        // horizontal collision
        float playerWidth = 0.3f;
        Vector3 testPos = position;
        testPos.x += moveVec.x;

        int wallX = (int)floor(testPos.x + (moveVec.x > 0 ? playerWidth : -playerWidth));
        int kneeY = (int)floor(position.y - 1.0f);
        int headY = (int)floor(position.y - 0.2f);
        int footZ = (int)floor(position.z);

        if (world.GetBlock(wallX, kneeY, footZ) == BlockType::AIR && world.GetBlock(wallX, headY, footZ) == BlockType::AIR) {
            position.x += moveVec.x;
        }

        testPos = position;
        testPos.z += moveVec.z;
        int wallZ = (int)floor(testPos.z + (moveVec.z > 0 ? playerWidth : -playerWidth));
        int currentFootX = (int)floor(position.x);

        if (world.GetBlock(currentFootX, kneeY, wallZ) == BlockType::AIR && world.GetBlock(currentFootX, headY, wallZ) == BlockType::AIR) {
            position.z += moveVec.z;
        }
    }

    camera.position = position;
    Vector3 lookDir = {
        sinf(cameraAngleX) * cosf(cameraAngleY),
        sinf(cameraAngleY),
        cosf(cameraAngleX) * cosf(cameraAngleY)
    };
    camera.target.x = camera.position.x + lookDir.x;
    camera.target.y = camera.position.y + lookDir.y;
    camera.target.z = camera.position.z + lookDir.z;
}

/**
 * casts a ray from the camera to detect selected block
 */
void Player::UpdateRaycast(ChunkManager& world) {
    isBlockSelected = false;
    Ray ray = GetMouseRay(Vector2{ (float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2 }, camera);
    float closestDist = 8.0f;

    int radius = 6;
    int camX = (int)floor(camera.position.x);
    int camY = (int)floor(camera.position.y);
    int camZ = (int)floor(camera.position.z);

    for (int x = camX - radius; x <= camX + radius; x++) {
        for (int y = camY - radius; y <= camY + radius; y++) {
            for (int z = camZ - radius; z <= camZ + radius; z++) {
                if (world.GetBlock(x, y, z, false) == BlockType::AIR) continue;

                Vector3 min = { (float)x, (float)y, (float)z };
                Vector3 max = { (float)x + 1.0f, (float)y + 1.0f, (float)z + 1.0f };
                BoundingBox box = { min, max };

                RayCollision collision = GetRayCollisionBox(ray, box);
                if (collision.hit && collision.distance < closestDist) {
                    isBlockSelected = true;
                    closestDist = collision.distance;
                    selectedBlockPos = Vector3{ (float)x, (float)y, (float)z };
                    selectedNormal = collision.normal;
                }
            }
        }
    }
}

/**
 * processes keyboard and mouse input for interaction
 */
void Player::HandleInput(ChunkManager& world) {
    // select hotbar
    if (IsKeyPressed(KEY_ONE))   inventory.selectedSlot = 0;
    if (IsKeyPressed(KEY_TWO))   inventory.selectedSlot = 1;
    if (IsKeyPressed(KEY_THREE)) inventory.selectedSlot = 2;
    if (IsKeyPressed(KEY_FOUR))  inventory.selectedSlot = 3;
    if (IsKeyPressed(KEY_FIVE))  inventory.selectedSlot = 4;
    if (IsKeyPressed(KEY_SIX))   inventory.selectedSlot = 5;
    if (IsKeyPressed(KEY_SEVEN)) inventory.selectedSlot = 6;
    if (IsKeyPressed(KEY_EIGHT)) inventory.selectedSlot = 7;
    if (IsKeyPressed(KEY_NINE))  inventory.selectedSlot = 8;

    float wheel = GetMouseWheelMove();
    if (wheel > 0) inventory.selectedSlot--;
    if (wheel < 0) inventory.selectedSlot++;
    if (inventory.selectedSlot < 0) inventory.selectedSlot = 8;
    if (inventory.selectedSlot > 8) inventory.selectedSlot = 0;

    if (isBlockSelected) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            world.SetBlock((int)selectedBlockPos.x, (int)selectedBlockPos.y, (int)selectedBlockPos.z, BlockType::AIR);
            UpdateRaycast(world);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            int currentID = GetHeldBlockID();
            if (currentID != 0) {
                int newX = (int)(selectedBlockPos.x + round(selectedNormal.x));
                int newY = (int)(selectedBlockPos.y + round(selectedNormal.y));
                int newZ = (int)(selectedBlockPos.z + round(selectedNormal.z));
                Vector3 blockCenter = { (float)newX + 0.5f, (float)newY + 0.5f, (float)newZ + 0.5f };
                if (Vector3Distance(camera.position, blockCenter) > 1.2f) {
                    world.SetBlock(newX, newY, newZ, (BlockType)currentID);
                    UpdateRaycast(world);
                }
            }
        }
    }
}