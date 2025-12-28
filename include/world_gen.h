#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "chunk_manager.h"
#include "raylib.h"

class WorldGen {
public:
    // main function called by ChunkManager
    static void GenerateChunk(Chunk& chunk, int chunkX, int chunkZ);

    // seed
    static int worldSeed;

private:
    // biome Types
    enum BiomeType { BIOME_FOREST, BIOME_DESERT, BIOME_SNOW };

    // helpers
    static BiomeType GetBiome(int x, int z);
    static void PlaceTree(Chunk& chunk, int x, int y, int z, BiomeType biome);
    static void PlaceCactus(Chunk& chunk, int x, int y, int z);

    // nise Helpers
    static float SimpleNoise3D(float x, float y, float z);
    static float GetHeightNoise(int x, int z);
};

#endif