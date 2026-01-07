#ifndef BLOCK_TYPES_H
#define BLOCK_TYPES_H

#include <cstdint>

/**
 * defines all available block types in the game
 * used for chunk data, rendering, and logic
 */
enum class BlockType : uint8_t {
    AIR = 0,
    DIRT = 1,
    STONE = 2,
    WOOD = 3,
    GRASS = 4,
    SAND = 5,
    BEDROCK = 6,
    LEAVES = 7,
    GRASS_SIDE = 8, // internal rendering use
    SNOW = 9,
    CACTUS = 10,
    SNOW_SIDE = 11, // internal rendering use
    SNOW_LEAVES = 12,
    TORCH = 13,    
    GLOWSTONE = 14,

    COUNT
};

/**
 * defines available biomes for world generation
 */
enum class BiomeType { 
    FOREST, 
    DESERT, 
    SNOW 
};

#endif