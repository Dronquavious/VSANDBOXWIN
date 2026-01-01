#ifndef BLOCK_TYPES_H
#define BLOCK_TYPES_H

enum BlockType {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_STONE = 2,
    BLOCK_WOOD = 3,
    BLOCK_GRASS = 4,
    BLOCK_SAND = 5,
    BLOCK_BEDROCK = 6,
    BLOCK_LEAVES = 7,
    BLOCK_GRASS_SIDE = 8, // internal use for rendering
    BLOCK_SNOW = 9,
    BLOCK_CACTUS = 10,
    BLOCK_SNOW_SIDE = 11  // internal use for rendering
};

enum BiomeType { 
    BIOME_FOREST, 
    BIOME_DESERT, 
    BIOME_SNOW 
};

#endif