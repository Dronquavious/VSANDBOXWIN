#include "world_gen.h"
int WorldGen::worldSeed = 0;

#include "raymath.h"
#include <cmath>

// --- NOISE HELPERS ---

float Fract(float x) { return x - floorf(x); }

float Hash3D(int x, int y, int z) {
    int h = x * 374761393 + y * 668265263 + z * 924083321;
    h = (h ^ (h >> 13)) * 1274126177;
    return (h & 0xFFFF) / 65535.0f;
}

// 3D Noise for Caves
float WorldGen::SimpleNoise3D(float x, float y, float z) {
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

// 2D Noise for Terrain Height
// a simple 2D Perlin wrapper here so we dont need to generate Images every chunk
// (gaster than GenImagePerlinNoise for single lookups)
float WorldGen::GetHeightNoise(int x, int z) {
    // TODO:
    return 0.0f;
}

// --- BIOME LOGIC ---


WorldGen::BiomeType WorldGen::GetBiome(int x, int z) {
    // use SimpleNoise3D for smooth, organic zones
    // scale 0.003f means biomes are ~300 blocks wide.
    // use 'x' and 'z' for the map, and '0' for height since it's 2D.
    float noise = SimpleNoise3D((x + worldSeed) * 0.003f, 0.0f, (z + worldSeed) * 0.003f);

    // simpleNoise3D returns a value between 0.0 and 1.0
    // we split this range into 3 biomes:
    if (noise < 0.4f) return BIOME_SNOW;   // Cold areas
    if (noise > 0.6f) return BIOME_DESERT; // Hot areas
    return BIOME_FOREST;                   // Middle areas
}

// --- GENERATION ---

void WorldGen::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
    int offsetX = chunkX * CHUNK_SIZE;
    int offsetZ = chunkZ * CHUNK_SIZE;

    // Raylibs Image noise for terrain height because it's convenient
    Image noiseImage = GenImagePerlinNoise(CHUNK_SIZE, CHUNK_SIZE, offsetX, offsetZ, 4.0f);
    Color* pixels = LoadImageColors(noiseImage);

    // PASS 1: TERRAIN
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {

            // determine Biome
            BiomeType biome = GetBiome(offsetX + x, offsetZ + z);

            // determine Height
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 4; // Base height

            for (int y = 0; y < CHUNK_SIZE; y++) {
                int blockType = 0;

                if (y == 0) blockType = 6; // Bedrock
                else if (y < height - 3) blockType = 2; // Stone
                else if (y < height) {
                    if (biome == BIOME_DESERT) blockType = 5; // Sand Underground
                    else blockType = 1; // Dirt Underground
                }
                else if (y == height) {
                    // top Block based on Biome
                    switch (biome) {
                    case BIOME_DESERT: blockType = 5; break; // Sand
                    case BIOME_SNOW:   blockType = 9; break; // Snow (NEW ID)
                    default:           blockType = 4; break; // Grass
                    }
                }

                // cave Cutout
                if (blockType == 2) {
                    float cave = SimpleNoise3D((offsetX + x) * 0.06f, y * 0.06f, (offsetZ + z) * 0.06f);
                    if (cave > 0.6f) blockType = 0;
                }

                chunk.blocks[x][y][z] = blockType;
            }
        }
    }

    // PASS 2: DECORATION (Trees / Cactus)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 4;

            int topBlock = chunk.blocks[x][height][z];

            // Random Chance
            if (GetRandomValue(0, 100) < 2) { // 2% chance
                // Padding check
                if (x > 2 && x < CHUNK_SIZE - 3 && z > 2 && z < CHUNK_SIZE - 3) {

                    BiomeType biome = GetBiome(offsetX + x, offsetZ + z);

                    if (biome == BIOME_DESERT && topBlock == 5) { // Sand
                        PlaceCactus(chunk, x, height + 1, z);
                    }
                    else if (biome == BIOME_FOREST && topBlock == 4) { // Grass
                        PlaceTree(chunk, x, height + 1, z, BIOME_FOREST);
                    }
                }
            }
        }
    }

    UnloadImageColors(pixels);
    UnloadImage(noiseImage);
}

void WorldGen::PlaceCactus(Chunk& chunk, int x, int y, int z) {
    int height = GetRandomValue(2, 4);
    for (int i = 0; i < height; i++) {
        if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = 10; // Cactus Block (NEW ID)
    }
}

void WorldGen::PlaceTree(Chunk& chunk, int x, int y, int z, BiomeType biome) {
    int height = GetRandomValue(4, 6);

    // Trunk
    for (int i = 0; i < height; i++) {
        if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = 3; // Wood
    }

    // Leaves (Simple Blob)
    // TODO: If Snow biome, we could make them pointier (Pine tree style) later
    // For now, let's keep it simple to ensure it works.
    int leafStart = height - 2;
    int leafEnd = height + 1;
    for (int ly = leafStart; ly <= leafEnd; ly++) {
        int radius = (ly == leafEnd) ? 1 : 2;
        for (int lx = -radius; lx <= radius; lx++) {
            for (int lz = -radius; lz <= radius; lz++) {
                if (abs(lx) == radius && abs(lz) == radius && (ly != leafEnd)) continue;
                if (chunk.blocks[x + lx][y + ly][z + lz] == 0) {
                    chunk.blocks[x + lx][y + ly][z + lz] = 7; // Leaves
                }
            }
        }
    }
}