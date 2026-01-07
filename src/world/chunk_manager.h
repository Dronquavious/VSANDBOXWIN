#ifndef CHUNK_MANAGER_H
#define CHUNK_MANAGER_H

#include "raylib.h"
#include "../core/constants.h"
#include "../blocks/block_types.h"
#include <map>
#include <vector>
#include <fstream>

/**
 * generic 32x32x32 voxel container
 * stores blocks, light data, and rendering mesh
 */
struct Chunk {
    BlockType blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    unsigned char light[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // packed light data
    Model layers[(int)BlockType::COUNT];
    bool meshReady;

    // physics flag (sleeping/awake)
    bool shouldStep;

    Chunk() {
        meshReady = false;
        shouldStep = false; // default to asleep
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    blocks[x][y][z] = BlockType::AIR;
                    light[x][y][z] = 15;
                }
            }
        }
        for (int i = 0; i < (int)BlockType::COUNT; i++) layers[i] = { 0 };
    }
};

/**
 * helper for chunk lookup in std::map
 */
struct ChunkCoord {
    int x, z;
    bool operator<(const ChunkCoord& other) const {
        if (x != other.x) return x < other.x;
        return z < other.z;
    }
};

/**
 * node for lighting bfs queue
 */
struct LightNode {
    int x, y, z;
    int val;
};

/**
 * manages all chunks, terrain generation, lighting, and physics
 */
class ChunkManager {
public:
    void Init();
    void UnloadAll();

    /**
     * renders visible chunks and handles loading logic
     */
    void UpdateAndDraw(Vector3 playerPos, Texture2D* textures, Shader shader, Color tint);

    /**
     * triggers a mesh rebuild for a specific chunk
     */
    void RebuildMesh(int cx, int cz, Texture2D* textures);

    /**
     * updates block physics (e.g. falling sand)
     */
    void UpdateChunkPhysics();

    BlockType GetBlock(int x, int y, int z, bool createIfMissing = true);
    int GetLightLevel(int x, int y, int z);
    
    /**
     * changes a block and updates neighbors/lighting
     */
    void SetBlock(int x, int y, int z, BlockType type);
    bool IsBlockSolid(int x, int y, int z);

    /**
     * writes world data to disk
     */
    void SaveChunks(std::ofstream& out);

    /**
     * reads world data from disk
     */
    void LoadChunks(std::ifstream& in);

private:
    std::map<ChunkCoord, Chunk> chunks;

    void GenerateChunk(Chunk& chunk, int chunkX, int chunkZ);
    void BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures);
    void UnloadChunkModels(Chunk& chunk);
    void ComputeChunkLighting(Chunk& chunk);
};

#endif