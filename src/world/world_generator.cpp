#include "world_generator.h"
#include "chunk_manager.h" 
#include "raymath.h"
#include <cstdlib>
#include "../blocks/block_types.h"

int WorldGenerator::worldSeed = 0;

// --- noise helpers ---

float Fract(float x) { return x - floorf(x); }

float Hash3D(int x, int y, int z) {
	int h = x * 374761393 + y * 668265263 + z * 924083321;
	h = (h ^ (h >> 13)) * 1274126177;
	return (h & 0xFFFF) / 65535.0f;
}

/**
 * 3d simplex-like noise function for caves and organic shapes
 */
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

/**
 * calculates terrain height using multiple noise layers
 * combines base terrain, mountains, and detail noise
 */
float WorldGenerator::GetHeightNoise(int x, int z) {
	// decides if we are in a "Flat" area or a "Hilly" area
	float roughness = SimpleNoise3D((x + worldSeed) * 0.005f, 0, (z + worldSeed) * 0.005f);

	// the actual bumps and hills
	float detail = SimpleNoise3D((x + worldSeed) * 0.02f, 100, (z + worldSeed) * 0.02f);

	// large, rare peaks
	float peaks = SimpleNoise3D((x + worldSeed) * 0.01f, 200, (z + worldSeed) * 0.01f);

	float finalHeight = 0;

	// TERRAIN SHAPING LOGIC
	if (roughness < 0.5f) {
		// LAINS (50% of world)
		// Very flat, minor bumps (+/- 2 blocks)
		finalHeight = SEA_LEVEL + (detail * 4.0f);
	}
	else {
		// MOUNTAINS (50% of world)
		// Transitions from hills to tall peaks
		// We blend the Detail and the Peak noise
		float mountainFactor = (roughness - 0.5f) * 2.0f; // 0.0 to 1.0 strength
		finalHeight = SEA_LEVEL + (detail * 5.0f) + (peaks * 30.0f * mountainFactor);
	}

	return finalHeight;
}

// --- biome logic ---

/**
 * determines biome type based on 2d noise map
 */
BiomeType WorldGenerator::GetBiome(int x, int z) {
	// use simplenoise3d for smooth, organic zones
	float noise = SimpleNoise3D((x + worldSeed) * 0.003f, 0.0f, (z + worldSeed) * 0.003f);

	if (noise < 0.4f) return BiomeType::SNOW;
	if (noise > 0.6f) return BiomeType::DESERT;
	return BiomeType::FOREST;
}

// --- generation ---

/**
 * main generation function: populates a chunk with blocks
 * pass 1: terrain & caves
 * pass 2: decorations (trees, cacti)
 */
void WorldGenerator::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
	int offsetX = chunkX * CHUNK_SIZE;
	int offsetZ = chunkZ * CHUNK_SIZE;

	// PASS 1: TERRAIN & CAVES
	for (int x = 0; x < CHUNK_SIZE; x++) {
		for (int z = 0; z < CHUNK_SIZE; z++) {
			int worldX = offsetX + x;
			int worldZ = offsetZ + z;

			BiomeType biome = GetBiome(worldX, worldZ);
			int height = (int)GetHeightNoise(worldX, worldZ);

			if (height < 1) height = 1;
			if (height >= CHUNK_SIZE) height = CHUNK_SIZE - 1;

			for (int y = 0; y < CHUNK_SIZE; y++) {
				BlockType blockType = BlockType::AIR;

				if (y == 0) blockType = BlockType::BEDROCK;
				else if (y < height - 3) blockType = BlockType::STONE;
				else if (y < height) {
					if (biome == BiomeType::DESERT) blockType = BlockType::SAND;
					else blockType = BlockType::DIRT;
				}
				else if (y == height) {
					switch (biome) {
					case BiomeType::DESERT: blockType = BlockType::SAND; break;
					case BiomeType::SNOW:   blockType = BlockType::SNOW; break;
					default:           blockType = BlockType::GRASS; break;
					}
				}

				// CAVE GENERATION
				if (blockType != BlockType::AIR && blockType != BlockType::BEDROCK && y > 3) {
					float caveNoise = SimpleNoise3D(worldX * 0.06f, y * 0.06f, worldZ * 0.06f);

					// DEPTH BIAS:
					// At y=0 (Bedrock), Bias is 0.0. Threshold is 0.65 (Common caves)
					// At y=64 (Sky), Bias is 1.0. Threshold is 1.15 (Impossible caves)
					float depthBias = (float)y / (float)CHUNK_SIZE;
					float threshold = 0.65f + (depthBias * 0.5f);

					if (caveNoise > threshold) blockType = BlockType::AIR;
				}

				chunk.blocks[x][y][z] = blockType;
			}
		}
	}

	// PASS 2: DECORATION
	for (int x = 0; x < CHUNK_SIZE; x++) {
		for (int z = 0; z < CHUNK_SIZE; z++) {

			int worldX = offsetX + x;
			int worldZ = offsetZ + z;

			// find Top Block
			int height = -1;
			for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
				if (chunk.blocks[x][y][z] != BlockType::AIR) {
					height = y;
					break;
				}
			}

			if (height <= 0 || height >= CHUNK_SIZE - 8) continue;

			BlockType topBlock = chunk.blocks[x][height][z];

			if (x > 2 && x < CHUNK_SIZE - 3 && z > 2 && z < CHUNK_SIZE - 3) {
				BiomeType biome = GetBiome(worldX, worldZ);

				if (GetRandomValue(0, 100) < 2) {
					if (biome == BiomeType::DESERT && topBlock == BlockType::SAND) {
						PlaceCactus(chunk, x, height + 1, z);
					}
					else if (biome == BiomeType::FOREST && topBlock == BlockType::GRASS) {
						PlaceTree(chunk, x, height + 1, z, BiomeType::FOREST);
					}
					else if (biome == BiomeType::SNOW && topBlock == BlockType::SNOW) {
						// exclusive snow tree
						PlaceSnowTree(chunk, x, height + 1, z);
					}
				}
			}
		}
	}
}

void WorldGenerator::PlaceCactus(Chunk& chunk, int x, int y, int z) {
	int height = GetRandomValue(2, 4);
	for (int i = 0; i < height; i++) {
		if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = BlockType::CACTUS;
	}
}

void WorldGenerator::PlaceTree(Chunk& chunk, int x, int y, int z, BiomeType biome) {
	int height = GetRandomValue(4, 6);

	// trunk
	for (int i = 0; i < height; i++) {
		if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = BlockType::WOOD;
	}

	// leaves
	int leafStart = height - 2;
	int leafEnd = height + 1;
	for (int ly = leafStart; ly <= leafEnd; ly++) {
		int radius = (ly == leafEnd) ? 1 : 2;
		for (int lx = -radius; lx <= radius; lx++) {
			for (int lz = -radius; lz <= radius; lz++) {
				if (abs(lx) == radius && abs(lz) == radius && (ly != leafEnd)) continue;

				// Safe Set
				int fx = x + lx;
				int fy = y + ly;
				int fz = z + lz;

				if (fx >= 0 && fx < CHUNK_SIZE && fy >= 0 && fy < CHUNK_SIZE && fz >= 0 && fz < CHUNK_SIZE) {
					if (chunk.blocks[fx][fy][fz] == BlockType::AIR) {
						chunk.blocks[fx][fy][fz] = BlockType::LEAVES;
					}
				}
			}
		}
	}
}

void WorldGenerator::PlaceSnowTree(Chunk& chunk, int x, int y, int z) {
	int height = GetRandomValue(6, 8); // taller than oak

	// trunk
	for (int i = 0; i < height; i++) {
		if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = BlockType::WOOD;
	}

	// conical leaves
	// from bottom branches to top
	// Layers: Wide -> Medium -> Small -> Top
	int startY = height - 5;
	if (startY < 0) startY = 0;

	for (int i = startY; i <= height; i++) {
		// radius gets smaller as we go up
		int radius = 2;
		if (i > height - 2) radius = 1;
		if (i == height) radius = 0; // Top tip

		for (int lx = -radius; lx <= radius; lx++) {
			for (int lz = -radius; lz <= radius; lz++) {
				// Circular/Conical check
				if (lx * lx + lz * lz > radius * radius + 1) continue;

				int fx = x + lx;
				int fy = y + i;
				int fz = z + lz;

				if (fx >= 0 && fx < CHUNK_SIZE && fy >= 0 && fy < CHUNK_SIZE && fz >= 0 && fz < CHUNK_SIZE) {
					if (chunk.blocks[fx][fy][fz] == BlockType::AIR) {
						chunk.blocks[fx][fy][fz] = BlockType::SNOW_LEAVES;
					}
				}
			}
		}
	}
}