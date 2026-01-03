#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "raylib.h"
#include "../core/constants.h"
#include <map>
#include <vector>

struct Chunk {
    int blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    unsigned char light[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // 15 full sun, 0 pitch black
    Model layers[13];
    bool meshReady;

    // optimization flag, if false phys ignores this chunk
    bool shouldStep;

    Chunk() {
        meshReady = false;
        shouldStep = false; // default to asleep
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    blocks[x][y][z] = 0;
                    light[x][y][z] = 15;
                }
            }
        }
        for (int i = 0; i < 13; i++) layers[i] = { 0 };
    }
};

struct ChunkCoord {
    int x, z;
    bool operator<(const ChunkCoord& other) const {
        if (x != other.x) return x < other.x;
        return z < other.z;
    }
};

struct LightNode {
    int x, y, z;
    int val;
};

class ChunkManager {
public:
    void Init();
    void UnloadAll();

    // draw and update
    void UpdateAndDraw(Vector3 playerPos, Texture2D* textures, Shader shader, Color tint);
    void UpdateChunkPhysics();

    // block access
    int GetBlock(int x, int y, int z, bool createIfMissing = true);
    int GetLightLevel(int x, int y, int z);
    void SetBlock(int x, int y, int z, int type);
    bool IsBlockSolid(int x, int y, int z);

private:
    std::map<ChunkCoord, Chunk> chunks;

    void GenerateChunk(Chunk& chunk, int chunkX, int chunkZ);
    void BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures);
    void UnloadChunkModels(Chunk& chunk);
    void ComputeChunkLighting(Chunk& chunk);
};

#endif