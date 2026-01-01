#include "world_generator.h"
#include "chunk_manager.h" 
#include "raymath.h"
#include <cmath>
#include <cstdlib>

int WorldGenerator::worldSeed = 0;

// --- noise helpers ---

float Fract(float x) { return x - floorf(x); }

float Hash3D(int x, int y, int z) {
    int h = x * 374761393 + y * 668265263 + z * 924083321;
    h = (h ^ (h >> 13)) * 1274126177;
    return (h & 0xFFFF) / 65535.0f;
}

// 3d noise for caves
float WorldGenerator::SimpleNoise3D(float x, float y, float z) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);
    float fx = Fract(x); float fy = Fract(y); float fz = Fract(z);
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);
    float w = fz * fz * (3.0f - 2.0f * fz);

    float c000 = Hash3D(ix, iy, iz); float c100 = Hash3D(ix + 1, iy, iz);
    float c010 = Hash3D(ix, iy + 1, iz); float c110 = Hash3D(ix + 1, iy + 1, iz);
    float c001 = Hash3D(ix, iy, iz + 1); float c101 = Hash3D(ix + 1, iy + 1, iz);
    float c011 = Hash3D(ix, iy + 1, iz + 1); float c111 = Hash3D(ix + 1, iy + 1, iz + 1);

    float x1 = Lerp(c000, c100, u); float x2 = Lerp(c010, c110, u);
    float y1 = Lerp(x1, x2, v);
    float x3 = Lerp(c001, c101, u); float x4 = Lerp(c011, c111, u);
    float y2 = Lerp(x3, x4, v);

    return Lerp(y1, y2, w);
}

float WorldGenerator::GetHeightNoise(int x, int z) {
    return 0.0f; 
}

// --- biome logic ---

BiomeType WorldGenerator::GetBiome(int x, int z) {
    // use simplenoise3d for smooth, organic zones
    float noise = SimpleNoise3D((x + worldSeed) * 0.003f, 0.0f, (z + worldSeed) * 0.003f);

    if (noise < 0.4f) return BIOME_SNOW;   
    if (noise > 0.6f) return BIOME_DESERT; 
    return BIOME_FOREST;                   
}

// --- generation ---

void WorldGenerator::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
    int offsetX = chunkX * CHUNK_SIZE;
    int offsetZ = chunkZ * CHUNK_SIZE;

    // raylibs image noise for terrain height because it's convenient
    Image noiseImage = GenImagePerlinNoise(CHUNK_SIZE, CHUNK_SIZE, offsetX, offsetZ, 4.0f);
    Color* pixels = LoadImageColors(noiseImage);

    // pass 1: terrain
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {

            BiomeType biome = GetBiome(offsetX + x, offsetZ + z);
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 4; // base height

            for (int y = 0; y < CHUNK_SIZE; y++) {
                int blockType = 0;

                if (y == 0) blockType = BLOCK_BEDROCK; 
                else if (y < height - 3) blockType = BLOCK_STONE;
                else if (y < height) {
                    if (biome == BIOME_DESERT) blockType = BLOCK_SAND; 
                    else blockType = BLOCK_DIRT;
                }
                else if (y == height) {
                    switch (biome) {
                        case BIOME_DESERT: blockType = BLOCK_SAND; break;
                        case BIOME_SNOW:   blockType = BLOCK_SNOW; break;
                        default:           blockType = BLOCK_GRASS; break;
                    }
                }

                // cave cutout
                if (blockType == BLOCK_STONE) {
                    float cave = SimpleNoise3D((offsetX + x) * 0.06f, y * 0.06f, (offsetZ + z) * 0.06f);
                    if (cave > 0.6f) blockType = BLOCK_AIR;
                }

                chunk.blocks[x][y][z] = blockType;
            }
        }
    }

    // pass 2: decoration
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 4;
            int topBlock = chunk.blocks[x][height][z];

            if (GetRandomValue(0, 100) < 2) { // 2% chance
                if (x > 2 && x < CHUNK_SIZE - 3 && z > 2 && z < CHUNK_SIZE - 3) {
                    BiomeType biome = GetBiome(offsetX + x, offsetZ + z);

                    if (biome == BIOME_DESERT && topBlock == BLOCK_SAND) {
                        PlaceCactus(chunk, x, height + 1, z);
                    }
                    else if (biome == BIOME_FOREST && topBlock == BLOCK_GRASS) {
                        PlaceTree(chunk, x, height + 1, z, BIOME_FOREST);
                    }
                }
            }
        }
    }

    UnloadImageColors(pixels);
    UnloadImage(noiseImage);
}

void WorldGenerator::PlaceCactus(Chunk& chunk, int x, int y, int z) {
    int height = GetRandomValue(2, 4);
    for (int i = 0; i < height; i++) {
        if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = BLOCK_CACTUS; 
    }
}

void WorldGenerator::PlaceTree(Chunk& chunk, int x, int y, int z, BiomeType biome) {
    int height = GetRandomValue(4, 6);

    // trunk
    for (int i = 0; i < height; i++) {
        if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = BLOCK_WOOD; 
    }

    // leaves
    int leafStart = height - 2;
    int leafEnd = height + 1;
    for (int ly = leafStart; ly <= leafEnd; ly++) {
        int radius = (ly == leafEnd) ? 1 : 2;
        for (int lx = -radius; lx <= radius; lx++) {
            for (int lz = -radius; lz <= radius; lz++) {
                if (abs(lx) == radius && abs(lz) == radius && (ly != leafEnd)) continue;
                if (chunk.blocks[x + lx][y + ly][z + lz] == 0) {
                    chunk.blocks[x + lx][y + ly][z + lz] = BLOCK_LEAVES; 
                }
            }
        }
    }
}