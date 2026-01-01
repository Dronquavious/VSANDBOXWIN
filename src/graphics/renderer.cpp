#define _CRT_SECURE_NO_WARNINGS 
#include "renderer.h"
#include "rlgl.h"
#include "../blocks/block_manager.h"
#include "../blocks/block_types.h"
#include "raymath.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// --- shader code ---

const char* fogVsCode = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matView;
out vec2 fragTexCoord;
out vec4 fragColor;
out float fragDist;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    vec4 viewPos = matView * matModel * vec4(vertexPosition, 1.0);
    fragDist = length(viewPos.xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* fogFsCode = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in float fragDist;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float fogDensity;
uniform vec3 fogColor;
void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texColor * colDiffuse * fragColor;
    float dist = max(fragDist - 70.0, 0.0);
    float fogFactor = 1.0 / exp(pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    finalColor = mix(vec4(fogColor, 1.0), baseColor, fogFactor);
}
)";

void Renderer::Init() {
    handBobbing = 0.0f;
    cloudScroll = 0.0f;

    // load block textures using blockmanager
    textures[BLOCK_DIRT] = BlockManager::GenDirtTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_STONE] = BlockManager::GenStoneTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_WOOD] = BlockManager::GenWoodTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_GRASS] = BlockManager::GenGrassTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_SAND] = BlockManager::GenSandTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_BEDROCK] = BlockManager::GenBedrockTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_LEAVES] = BlockManager::GenLeavesTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_GRASS_SIDE] = BlockManager::GenGrassSideTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_SNOW] = BlockManager::GenSnowTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_CACTUS] = BlockManager::GenCactusTexture(BLOCK_TEX_SIZE);
    textures[BLOCK_SNOW_SIDE] = BlockManager::GenSnowSideTexture(BLOCK_TEX_SIZE);

    Mesh mesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    blockModel = LoadModelFromMesh(mesh);

    // shader setup
    fogShader = LoadShaderFromMemory(fogVsCode, fogFsCode);
    fogDensityLoc = GetShaderLocation(fogShader, "fogDensity");
    fogColorLoc = GetShaderLocation(fogShader, "fogColor");
    float density = 0.005f;
    SetShaderValue(fogShader, fogDensityLoc, &density, SHADER_UNIFORM_FLOAT);

    // sky setup
    Mesh skyMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    skyModel = LoadModelFromMesh(skyMesh);
    Texture2D skyTex = GenerateSkyTexture();
    skyModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTex;
    SetTextureWrap(skyTex, TEXTURE_WRAP_CLAMP);

    // cloud setup
    Mesh cloudMesh = GenMeshPlane(2000.0f, 2000.0f, 1, 1);
    for (int i = 0; i < cloudMesh.vertexCount; i++) {
        cloudMesh.texcoords[i * 2] *= 8.0f;     
        cloudMesh.texcoords[i * 2 + 1] *= 8.0f; 
    }
    cloudModel = LoadModelFromMesh(cloudMesh);
    texClouds = GenerateCloudTexture();
    cloudModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texClouds;

    // haze setup
    Mesh hazeMesh = GenMeshCylinder(390.0f, 200.0f, 16);
    hazeModel = LoadModelFromMesh(hazeMesh);
    texHaze = GenerateHazeTexture();
    hazeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texHaze;
}

void Renderer::Unload() {
    for (int i = 1; i <= 11; i++) UnloadTexture(textures[i]);
    UnloadModel(blockModel);
    UnloadModel(skyModel);
    UnloadModel(cloudModel);
    UnloadModel(hazeModel);
    UnloadTexture(texClouds);
    UnloadTexture(texHaze);
    UnloadShader(fogShader);
}

void Renderer::DrawScene(Player& player, ChunkManager& world, float timeOfDay) {
    float brightness = 1.0f;
    if (timeOfDay > 0.5f) {
        float t = (timeOfDay - 0.5f) * 2.0f;
        brightness = 1.0f - t;
    } else {
        float t = timeOfDay * 2.0f;
        brightness = t;
    }
    if (brightness < 0.1f) brightness = 0.1f;

    Color skyTint = {
        (unsigned char)(255 * brightness),
        (unsigned char)(255 * brightness),
        (unsigned char)(255 * brightness),
        255
    };

    cloudScroll += GetFrameTime() * 0.005f;
    if (cloudScroll > 1000.0f) cloudScroll = 0.0f;

    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode3D(player.camera);

    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();

    Vector3 skyScale = { -800.0f, 800.0f, 800.0f };
    DrawModelEx(skyModel, player.camera.position, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, skyScale, skyTint);

    float orbitRadius = 400.0f;
    float angle = (timeOfDay * 2.0f * PI) - (PI / 2.0f);
    Vector3 sunPos = { player.camera.position.x + cosf(angle) * orbitRadius, player.camera.position.y + sinf(angle) * orbitRadius, player.camera.position.z };
    Vector3 moonPos = { player.camera.position.x - cosf(angle) * orbitRadius, player.camera.position.y - sinf(angle) * orbitRadius, player.camera.position.z };
    DrawSphere(sunPos, 40.0f, Color{ 255,255,200,255 });
    DrawSphere(moonPos, 20.0f, Color{ 220,220,220,255 });

    rlEnableDepthTest();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();

    Vector3 camPos = player.camera.position;
    Vector3 cloudPos = camPos;
    cloudPos.y = camPos.y + 80.0f;
    static float cloudAngle = 0.0f;
    cloudAngle += GetFrameTime() * 2.0f;
    if (cloudAngle > 360.0f) cloudAngle -= 360.0f;

    float sunHeight = sinf(angle);
    float dayFactor = Clamp((sunHeight + 0.2f) / 1.2f, 0.0f, 1.0f);
    Color cloudDay = Color{ 240, 240, 240, 255 };
    Color cloudNight = Color{ 80, 90, 110, 255 };
    Color cloudTint = LerpColor(cloudNight, cloudDay, dayFactor);

    DrawModelEx(cloudModel, cloudPos, Vector3{ 0, 1, 0 }, cloudAngle, Vector3{ 1, 1, 1 }, Fade(cloudTint, 0.9f));
    rlEnableDepthMask();
    EndBlendMode();

    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    Color skyDay = Color{ 135, 180, 235, 255 };
    Color skyNight = Color{ 10, 15, 30, 255 };
    Color skyColor = LerpColor(skyNight, skyDay, dayFactor);
    Vector3 hazePos = camPos;
    hazePos.y = camPos.y + 50.0f;
    DrawModelEx(hazeModel, hazePos, Vector3 {0, 1, 0}, 0.0f, Vector3 {1, 1, 1}, Fade(skyColor, 0.9f));
    rlEnableDepthMask();
    EndBlendMode();

    rlEnableBackfaceCulling();

    // update shader
    float fogColor[3] = { skyColor.r / 255.0f, skyColor.g / 255.0f, skyColor.b / 255.0f };
    SetShaderValue(fogShader, fogColorLoc, fogColor, SHADER_UNIFORM_VEC3);

    world.UpdateAndDraw(player.position, textures, fogShader, skyTint);

    if (player.isBlockSelected) {
        Vector3 center = {
            player.selectedBlockPos.x + 0.5f,
            player.selectedBlockPos.y + 0.5f,
            player.selectedBlockPos.z + 0.5f
        };
        DrawCubeWires(center, 1.01f, 1.01f, 1.01f, BLACK);
    }

    DrawHand(player, skyTint);
    EndMode3D();
}

void Renderer::DrawHand(Player& player, Color tint) {
    bool isMoving = (IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D));
    if (isMoving) handBobbing += GetFrameTime() * 10.0f;
    else handBobbing = Lerp(handBobbing, 0.0f, 0.1f);

    float bobOffset = sinf(handBobbing) * 0.1f;
    Vector3 handPos = player.camera.position;
    handPos.x += player.forward.x * 0.8f;
    handPos.z += player.forward.z * 0.8f;
    handPos.x -= player.right.x * 0.5f;
    handPos.z -= player.right.z * 0.5f;
    handPos.y -= 0.4f;
    handPos.y += bobOffset;

    int currentID = player.GetHeldBlockID();
    Texture2D handTexture = textures[1];
    if (currentID > 0 && currentID <= 11) handTexture = textures[currentID];

    blockModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = handTexture;
    Vector3 scale = { 0.4f, 0.4f, 0.4f };
    Vector3 rotationAxis = { 0.0f, 0.0f, 1.0f }; 
    float rotationAngle = 180.0f;                
    DrawModelEx(blockModel, handPos, rotationAxis, rotationAngle, scale, tint);
}

void Renderer::DrawUI(Player& player, int screenWidth, int screenHeight, const char* msg, float msgTimer) {
    int cx = screenWidth / 2;
    int cy = screenHeight / 2;
    DrawLine(cx - 10, cy, cx + 10, cy, BLACK);
    DrawLine(cx, cy - 10, cx, cy + 10, BLACK);
    DrawPixel(cx, cy, RED);
    DrawFPS(10, 10);

    if (msgTimer > 0.0f) {
        float alpha = 1.0f;
        if (msgTimer < 1.0f) alpha = msgTimer;
        int textWidth = MeasureText(msg, 30);
        int centerX = screenWidth / 2 - textWidth / 2;
        DrawText(msg, centerX, 50, 30, Fade(GREEN, alpha));
    }

    // hotbar
    int blockSize = 50;
    int padding = 10;
    int numSlots = 9;
    int totalWidth = (blockSize * numSlots) + (padding * (numSlots - 1));
    int startX = (screenWidth - totalWidth) / 2;
    int startY = screenHeight - blockSize - 20;

    for (int i = 0; i < numSlots; i++) {
        int x = startX + (i * (blockSize + padding));
        Color color = Fade(LIGHTGRAY, 0.5f);
        if (player.inventory.selectedSlot == i) color = YELLOW;

        DrawRectangle(x - 2, startY - 2, blockSize + 4, blockSize + 4, BLACK);
        DrawRectangle(x, startY, blockSize, blockSize, color);

        int blockID = player.inventory.slots[i].blockID;
        if (blockID != 0) {
            Texture2D previewTex = textures[1];
            // map real block ID to texture ID (some might share)
            if (blockID == BLOCK_GRASS) previewTex = textures[BLOCK_GRASS_SIDE];
            else if (blockID == BLOCK_SNOW) previewTex = textures[BLOCK_SNOW_SIDE];
            else if (blockID <= 11) previewTex = textures[blockID];

            Rectangle sourceRec = { 0.0f, 0.0f, (float)previewTex.width, (float)previewTex.height };
            Rectangle destRec = { (float)x + 4, (float)startY + 4, (float)blockSize - 8, (float)blockSize - 8 };
            DrawTexturePro(previewTex, sourceRec, destRec, Vector2{ 0,0 }, 0.0f, WHITE);
        }
        DrawText(TextFormat("%d", i + 1), x + 2, startY + 2, 10, BLACK);
    }
}

void Renderer::DrawDebug(Player& player, float& daySpeed, int& timeMode) {
    int width = 280;
    int height = 210;
    int x = GetScreenWidth() - width - 10; 
    int y = 10;                            

    DrawRectangle(x, y, width, height, Fade(BLACK, 0.85f));
    DrawRectangleLines(x, y, width, height, WHITE);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);

    GuiSlider(Rectangle{ (float)x + 100, (float)y + 30, 120, 20 }, "Gravity", TextFormat("%2.3f", player.gravity), &player.gravity, 0.001f, 0.1f);
    GuiSlider(Rectangle{ (float)x + 100, (float)y + 60, 120, 20 }, "Jump Power", TextFormat("%2.2f", player.jumpForce), &player.jumpForce, 0.1f, 1.0f);
    GuiSlider(Rectangle{ (float)x + 100, (float)y + 90, 120, 20 }, "Walk Speed", TextFormat("%2.1f", player.moveSpeed), &player.moveSpeed, 1.0f, 20.0f);
    GuiSlider(Rectangle{ (float)x + 100, (float)y + 120, 120, 20 }, "Fly Speed", TextFormat("%2.1f", player.flySpeed), &player.flySpeed, 5.0f, 50.0f);
    GuiSlider(Rectangle{ (float)x + 100, (float)y + 150, 120, 20 }, "Time Speed", TextFormat("%2.4f", daySpeed), &daySpeed, 0.0f, 0.1f);
    GuiToggleGroup(Rectangle{ (float)x + 100, (float)y + 180, 40, 20 }, "AUTO;DAY;NIGHT", &timeMode);

    DrawText("Time Mode", x + 20, y + 185, 10, WHITE);
    DrawText("Press TAB to Close", x + 20, y + 220, 10, WHITE);
}

Texture2D Renderer::GenerateSkyTexture() {
    const int width = 64;
    const int height = 512;
    Image img = GenImageColor(width, height, BLACK);
    Color* pixels = (Color*)MemAlloc(width * height * sizeof(Color));
    Color zenith = { 100, 160, 255, 255 };
    Color horizon = { 220, 240, 255, 255 };
    Color voidCol = { 20, 20, 45, 255 };

    for (int y = 0; y < height; y++) {
        float t = (float)y / (float)(height - 1);
        float curve = powf(t, 1.3f);
        Color rowColor;
        if (curve < 0.55f) {
            float localT = curve / 0.55f;
            rowColor = LerpColor(zenith, horizon, localT);
        } else {
            float localT = (curve - 0.55f) / 0.45f;
            rowColor = LerpColor(horizon, voidCol, localT);
        }
        float glow = expf(-powf((t - 0.5f) * 6.0f, 2.0f));
        rowColor.r = Clamp(rowColor.r + glow * 12, 0, 255);
        rowColor.g = Clamp(rowColor.g + glow * 12, 0, 255);
        rowColor.b = Clamp(rowColor.b + glow * 6, 0, 255);
        for (int x = 0; x < width; x++) {
            float noise = ((x * 13 + y * 7) % 17) / 255.0f;
            Color finalColor = rowColor;
            finalColor.r = Clamp(finalColor.r + noise * 6, 0, 255);
            finalColor.g = Clamp(finalColor.g + noise * 6, 0, 255);
            finalColor.b = Clamp(finalColor.b + noise * 6, 0, 255);
            pixels[y * width + x] = finalColor;
        }
    }
    RL_FREE(img.data);
    img.data = pixels;
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

Texture2D Renderer::GenerateCloudTexture() {
    const int size = 512;
    Image img = GenImageColor(size, size, BLANK);
    Color* pixels = LoadImageColors(img);
    for (int i = 0; i < size * size; i++) pixels[i] = BLANK;

    for (int i = 0; i < 2000; i++) {
        int x = GetRandomValue(0, size - 1);
        int y = GetRandomValue(0, size - 1);
        int radius = GetRandomValue(20, 60);
        int baseOpacity = GetRandomValue(10, 30);
        for (int py = -radius; py <= radius; py++) {
            for (int px = -radius; px <= radius; px++) {
                if (px * px + py * py > radius * radius) continue;
                float dist = sqrtf(float(px * px + py * py)) / radius;
                int blobAlpha = int((1.0f - dist) * baseOpacity);
                if (blobAlpha < 0) blobAlpha = 0;
                int finalX = (x + px + size) % size;
                int finalY = (y + py + size) % size;
                Color* target = &pixels[finalY * size + finalX];
                int newAlpha = target->a + blobAlpha;
                if (newAlpha > 255) newAlpha = 255;
                int shade = 220 + GetRandomValue(0, 35);
                target->r = shade;
                target->g = shade;
                target->b = shade;
                target->a = (unsigned char)newAlpha;
            }
        }
    }
    UnloadImage(img);
    Image cloudImg = { pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D tex = LoadTextureFromImage(cloudImg);
    UnloadImageColors(pixels);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    return tex;
}

Texture2D Renderer::GenerateHazeTexture() {
    const int width = 32;
    const int height = 128;
    Image img = GenImageColor(width, height, BLANK);
    Color* pixels = LoadImageColors(img);
    for (int y = 0; y < height; y++) {
        float t = (float)y / (float)(height - 1);
        float alpha = powf(t, 4.0f) * 120.0f;
        for (int x = 0; x < width; x++) {
            pixels[y * width + x] = { 255, 255, 255, (unsigned char)alpha };
        }
    }
    Image finalImg = { pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D tex = LoadTextureFromImage(finalImg);
    UnloadImageColors(pixels);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    return tex;
}

Color Renderer::LerpColor(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return Color{
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}