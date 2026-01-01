#include "block_manager.h"
#include "raymath.h"
#include <cmath>

// helper for min/max
#define MIN(a,b) ((a) < (b) ? (a) : (b))

Texture2D BlockManager::GenStoneTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 0, 0, 8.0f);
    ImageColorBrightness(&img, -40);
    ImageColorContrast(&img, -30);
    ImageColorTint(&img, Color{ 200, 200, 200, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

Texture2D BlockManager::GenWoodTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 0, 0, 4.0f);
    ImageColorBrightness(&img, -10);
    ImageColorContrast(&img, -20);
    ImageColorTint(&img, Color{ 170, 135, 85, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenSandTexture(int size) {
    Image img = GenImageColor(size, size, BLANK);
    Color* pixels = LoadImageColors(img);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            Color sand = { 237, 229, 173, 255 };
            int nx = x / 2;
            int ny = y / 2;
            unsigned int seed = nx * 49632 ^ ny * 325176;
            seed = (seed << 13) ^ seed;
            float noise = 1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
            noise = roundf(noise * 4.0f) / 4.0f;
            int variation = (int)(noise * 8.0f);
            sand.r = Clamp(sand.r + variation, 0, 255);
            sand.g = Clamp(sand.g + variation, 0, 255);
            sand.b = Clamp(sand.b + variation, 0, 255);

            if (GetRandomValue(0, 100) < 6) {
                sand.r -= 10;
                sand.g -= 10;
                sand.b -= 15;
            }

            // if we are at the edge of the block, darken the color
            if (x == 0 || x == size - 1 || y == 0 || y == size - 1) {
                float edgeNoise = 0.92f + GetRandomValue(0, 4) * 0.01f;
                sand.r *= edgeNoise;
                sand.g *= edgeNoise;
                sand.b *= edgeNoise;
            }
            pixels[y * size + x] = sand;
        }
    }

    Image newImg;
    newImg.data = pixels;
    newImg.width = size;
    newImg.height = size;
    newImg.mipmaps = 1;
    newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D tex = LoadTextureFromImage(newImg);
    UnloadImageColors(pixels);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenLeavesTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 0, 0, 5.0f);
    ImageColorBrightness(&img, -15);
    ImageColorContrast(&img, -10);
    ImageColorTint(&img, Color{ 70, 140, 60, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenBedrockTexture(int size) {
    Image img = GenImageColor(size, size, BLANK);
    Color* pixels = LoadImageColors(img);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            Color rock = { 55, 55, 55, 255 };
            int nx = x / 3;
            int ny = y / 3;
            unsigned int seed = nx * 92837111 ^ ny * 689287499;
            seed = (seed << 13) ^ seed;
            float noise1 = 1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
            unsigned int seed2 = x * 73428767 ^ y * 912931;
            seed2 = (seed2 << 13) ^ seed2;
            float noise2 = 1.0f - ((seed2 * (seed2 * seed2 * 12497 + 604727) + 1345679039) & 0x7fffffff) / 1073741824.0f;
            float combined = (noise1 * 0.7f + noise2 * 0.3f);
            combined = roundf(combined * 5.0f) / 5.0f;
            int variation = (int)(combined * 35.0f);
            rock.r = Clamp(rock.r + variation, 20, 110);
            rock.g = Clamp(rock.g + variation, 20, 110);
            rock.b = Clamp(rock.b + variation, 20, 110);
            if (noise2 < -0.65f) rock = { 15, 15, 15, 255 };
            pixels[y * size + x] = rock;
        }
    }
    Image newImg;
    newImg.data = pixels;
    newImg.width = size;
    newImg.height = size;
    newImg.mipmaps = 1;
    newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D tex = LoadTextureFromImage(newImg);
    UnloadImageColors(pixels);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenDirtTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f);
    ImageColorBrightness(&img, -30);
    ImageColorContrast(&img, -10);
    ImageColorTint(&img, Color{ 150, 100, 70, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenGrassTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 100, 100, 8.0f);
    ImageColorBrightness(&img, -10);
    ImageColorContrast(&img, -20);
    ImageColorTint(&img, Color{ 120, 200, 80, 255 });
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenGrassSideTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f);
    ImageColorBrightness(&img, -30);
    ImageColorContrast(&img, -10);
    ImageColorTint(&img, Color{ 150, 100, 70, 255 });
    Color grassColor = { 120, 200, 80, 255 };
    ImageDrawRectangle(&img, 0, 0, size, size / 3, grassColor);
    for (int i = 0; i < size; i++) {
        if (GetRandomValue(0, 1)) {
            ImageDrawPixel(&img, i, size / 3, grassColor);
            if (GetRandomValue(0, 1)) ImageDrawPixel(&img, i, (size / 3) + 1, grassColor);
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenSnowTexture(int size) {
    Image img = GenImageColor(size, size, BLANK);
    Color* pixels = LoadImageColors(img);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            Color snow = { 240, 242, 245, 255 };
            int nx = x / 2;
            int ny = y / 2;
            unsigned int seed = nx * 73856093 ^ ny * 19349663;
            seed = (seed << 13) ^ seed;
            float noise = 1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
            noise = roundf(noise * 3.0f) / 3.0f;
            int variation = (int)(noise * 4.0f);
            snow.r = Clamp(snow.r + variation, 0, 255);
            snow.g = Clamp(snow.g + variation, 0, 255);
            snow.b = Clamp(snow.b + variation, 0, 255);
            int edgeDist = MIN(MIN(x, size - 1 - x), MIN(y, size - 1 - y));
            if (edgeDist == 0) {
                float edge = 0.97f;
                snow.r *= edge;
                snow.g *= edge;
                snow.b *= edge;
            }
            pixels[y * size + x] = snow;
        }
    }
    Image newImg;
    newImg.data = pixels;
    newImg.width = size;
    newImg.height = size;
    newImg.mipmaps = 1;
    newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D tex = LoadTextureFromImage(newImg);
    UnloadImageColors(pixels);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenCactusTexture(int size) {
    Image img = GenImageColor(size, size, Color{ 60, 140, 60, 255 });
    for (int i = 2; i < size; i += 4) {
        ImageDrawRectangle(&img, i, 0, 1, size, Color{ 80, 180, 80, 255 });
    }
    for (int i = 0; i < 10; i++) {
        int rx = GetRandomValue(0, size - 1);
        int ry = GetRandomValue(0, size - 1);
        ImageDrawPixel(&img, rx, ry, BLACK);
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

Texture2D BlockManager::GenSnowSideTexture(int size) {
    Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f);
    ImageColorBrightness(&img, -30);
    ImageColorContrast(&img, -10);
    ImageColorTint(&img, Color{ 150, 100, 70, 255 });
    Color snowColor = { 240, 242, 245, 255 };
    ImageDrawRectangle(&img, 0, 0, size, size / 3, snowColor);
    for (int i = 0; i < size; i++) {
        if (GetRandomValue(0, 1)) {
            ImageDrawPixel(&img, i, size / 3, snowColor);
            if (GetRandomValue(0, 1)) ImageDrawPixel(&img, i, (size / 3) + 1, snowColor);
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}