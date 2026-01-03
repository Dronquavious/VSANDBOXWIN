#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"
#include "../player/player.h"
#include "../world/chunk_manager.h"

class Renderer {
public:
    void Init();
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

    // environment models
    Model skyModel;
    Model cloudModel;
    Model hazeModel;
    Model blockModel; // used for hand/ui

    // environment textures
    Texture2D texClouds;
    Texture2D texHaze;
    Texture2D textures[15]; // block textures

    // animation state
    float cloudScroll;
    float handBobbing;

    // internal helpers
    Texture2D GenerateSkyTexture();
    Texture2D GenerateCloudTexture();
    Texture2D GenerateHazeTexture();
    Color LerpColor(Color a, Color b, float t);
    void DrawHand(Player& player, Color tint);
};

#endif