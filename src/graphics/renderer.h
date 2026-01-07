#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"
#include "../player/player.h"
#include "../world/chunk_manager.h"
#include "../blocks/block_types.h"

/**
 * handles all rendering operations for the game, including
 * world rendering, ui, and debug overlays.
 * manages shaders, textures, and models.
 */
class Renderer {
public:
    /**
     * initializes rendering resources (textures, shaders, models)
     */
    void Init();

    /**
     * cleans up and unloads all rendering resources
     */
    void Unload();

    // main draw calls
    void DrawScene(Player& player, ChunkManager& world, float timeOfDay);
    void DrawUI(Player& player, int screenWidth, int screenHeight, const char* msg, float msgTimer);
    void DrawDebug(Player& player, float& daySpeed, int& timeMode);

    Texture2D* GetTextures() { return textures; }

private:
    // shaders
    Shader fogShader;
    int fogDensityLoc;
    int fogColorLoc;
    int sunBrightnessLoc;

    // dynamic light uniforms
    int playerLightPosLoc;
    int playerLightStrengthLoc;

    // environment models
    Model skyModel;
    Model cloudModel;
    Model hazeModel;
    Model blockModel; // used for hand/ui

    // environment textures
    Texture2D texClouds;
    Texture2D texHaze;
    Texture2D textures[(int)BlockType::COUNT]; // block textures

    // animation state
    float cloudScroll;
    float handBobbing;

    // internal helpers
    Texture2D GenerateSkyTexture();
    Texture2D GenerateCloudTexture();
    Texture2D GenerateHazeTexture();
    /**
 * utility to linearly interpolate between two colors
 */
Color LerpColor(Color a, Color b, float t);
    void DrawHand(Player& player, Color tint);
};

#endif