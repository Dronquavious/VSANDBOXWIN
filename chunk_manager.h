#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "raylib.h"
#include <map>
#include <vector>

#define CHUNK_SIZE 32

struct Chunk {
	int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

	// store the "Baked" 3D models here
	// layers[1] = Dirt Model, layers[2] = Stone Model, etc.
	Model layers[9];
	bool meshReady; // is mesh built and ready to draw?

	Chunk() {
		meshReady = false;
		// initialize to air
		for (int x = 0; x < CHUNK_SIZE; x++)
			for (int y = 0; y < CHUNK_SIZE; y++)
				for (int z = 0; z < CHUNK_SIZE; z++)
					blocks[x][y][z] = 0;

		// initialize Empty Models
		for (int i = 0; i < 9; i++) layers[i] = { 0 };
	}
};

struct ChunkCoord {
	int x, z;
	bool operator<(const ChunkCoord& other) const {
		if (x != other.x) return x < other.x;
		return z < other.z;
	}
};

class ChunkManager {
public:
	void Init();

	// Draw
	void UpdateAndDraw(Vector3 playerPos, Texture2D* textures);

	// Block Access
	int GetBlock(int x, int y, int z, bool createIfMissing = true);
	void SetBlock(int x, int y, int z, int type);

	// Helpers
	bool IsBlockSolid(int x, int y, int z);
	void UnloadAll(); // cleanup function

private:
	std::map<ChunkCoord, Chunk> chunks;

	void GenerateChunk(Chunk& chunk, int chunkX, int chunkZ);

	void CreateTree(Chunk& chunk, int x, int y, int z);

	// function that does the heavy math ONCE
	void BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures);
	void UnloadChunkModels(Chunk& chunk);
};

#endif