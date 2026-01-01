#ifndef WORLD_GENERATOR_H
#define WORLD_GENERATOR_H

#include "../core/constants.h"
#include "../blocks/block_types.h"

// forward declaration to avoid circular includes
struct Chunk; 

class WorldGenerator {
public:
    static void GenerateChunk(Chunk& chunk, int chunkX, int chunkZ);
    static int worldSeed;

private:
    static BiomeType GetBiome(int x, int z);
    static void PlaceTree(Chunk& chunk, int x, int y, int z, BiomeType biome);
    static void PlaceCactus(Chunk& chunk, int x, int y, int z);
    
    // noise helpers
    static float SimpleNoise3D(float x, float y, float z);
    static float GetHeightNoise(int x, int z);
};

#endif