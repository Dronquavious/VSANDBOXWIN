#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include "raylib.h"

/**
 * static utility class for procedural texture generation
 * creates textures for blocks at runtime
 */
class BlockManager {
public:
    // procedural texture generators
    static Texture2D GenStoneTexture(int size);
    static Texture2D GenDirtTexture(int size);
    static Texture2D GenGrassTexture(int size);
    static Texture2D GenWoodTexture(int size);
    static Texture2D GenSandTexture(int size);
    static Texture2D GenBedrockTexture(int size);
    static Texture2D GenLeavesTexture(int size);
    static Texture2D GenGrassSideTexture(int size);
    static Texture2D GenSnowTexture(int size);
    static Texture2D GenCactusTexture(int size);
    static Texture2D GenSnowSideTexture(int size);
    static Texture2D GenSnowLeavesSideTexture(int size);
    static Texture2D GenTorchTexture(int size);
    static Texture2D GenGlowstoneTexture(int size);
};

#endif