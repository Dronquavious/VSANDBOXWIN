#include "chunk_manager.h"
#include "world_generator.h"
#include "raymath.h"
#include "rlgl.h"
#include "../blocks/block_types.h"
#include <cmath>
#include <vector>
#include <cstring>
#include <queue>

void ChunkManager::Init() {}

void ChunkManager::UnloadAll() {
    for (auto& pair : chunks) {
        UnloadChunkModels(pair.second);
    }
    chunks.clear();
}

/**
 * frees all gpu models associated with the chunk
 */
void ChunkManager::UnloadChunkModels(Chunk& chunk) {
    for (int i = 0; i < (int)BlockType::COUNT; i++) {
        if (chunk.layers[i].meshCount > 0) {
            UnloadModel(chunk.layers[i]);
            chunk.layers[i] = { 0 };
        }
    }
    chunk.meshReady = false;
}

/**
 * retrieves a block type from global coordinates
 */
BlockType ChunkManager::GetBlock(int x, int y, int z, bool createIfMissing) {
    if (y < 0 || y >= CHUNK_SIZE) return BlockType::AIR;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) {
        if (createIfMissing) GenerateChunk(chunks[coord], cx, cz);
        else return BlockType::AIR;
    }

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    return chunks[coord].blocks[lx][y][lz];
}

/**
 * sets a block and updates lighting/meshes
 */
void ChunkManager::SetBlock(int x, int y, int z, BlockType type) {
    if (y < 0 || y >= CHUNK_SIZE) return;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) GenerateChunk(chunks[coord], cx, cz);

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    // update the block
    chunks[coord].blocks[lx][y][lz] = type;

    // recalculate lighting
    ComputeChunkLighting(chunks[coord]);

    // rebuild mesh
    chunks[coord].meshReady = false;
    chunks[coord].shouldStep = true;
    
    // update neighbors
    if (lx == 0) RebuildMesh(cx - 1, cz, nullptr);
    if (lx == CHUNK_SIZE - 1) RebuildMesh(cx + 1, cz, nullptr);
    if (lz == 0) RebuildMesh(cx, cz - 1, nullptr);
    if (lz == CHUNK_SIZE - 1) RebuildMesh(cx, cz + 1, nullptr);
}

bool ChunkManager::IsBlockSolid(int x, int y, int z) {
    return GetBlock(x, y, z) != BlockType::AIR;
}

void ChunkManager::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
    WorldGenerator::GenerateChunk(chunk, chunkX, chunkZ);
    ComputeChunkLighting(chunk);

    // wake up the chunk so floating sand can settle
    chunk.shouldStep = true;
}

void ChunkManager::ComputeChunkLighting(Chunk& chunk) {
    // CLEAR LIGHTING (Reset to 0)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                chunk.light[x][y][z] = 0;
            }
        }
    }

    std::queue<LightNode> sunQueue;
    std::queue<LightNode> torchQueue;

    // INITIALIZE SOURCES
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {

            // SUNLIGHT (Column Scan)
            bool sunBlocked = false;
            for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
                BlockType block = chunk.blocks[x][y][z];
                // Light passes through Air, Leaves, and Torches
                bool solid = (block != BlockType::AIR && block != BlockType::LEAVES && block != BlockType::SNOW_LEAVES && block != BlockType::TORCH && block != BlockType::GLOWSTONE);

                if (!sunBlocked) {
                    if (solid) {
                        sunBlocked = true;
                    }
                    else {
                        // Set Sun Bit (High Nibble) to 15 -> (15 << 4) = 240
                        chunk.light[x][y][z] |= (15 << 4);
                        sunQueue.push({ x, y, z, 15 });
                    }
                }
            }

            // TORCHLIGHT (Scan for emitters)
            for (int y = 0; y < CHUNK_SIZE; y++) {
                BlockType block = chunk.blocks[x][y][z];
                if (block == BlockType::TORCH || block == BlockType::GLOWSTONE) {
                    // Set Torch Bit (Low Nibble) to 14 (Torches aren't fully 15 bright usually)
                    chunk.light[x][y][z] |= 14;
                    torchQueue.push({ x, y, z, 14 });
                }
            }
        }
    }

    // SPREAD SUNLIGHT (BFS)
    while (!sunQueue.empty()) {
        LightNode node = sunQueue.front();
        sunQueue.pop();

        int neighbors[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

        for (int i = 0; i < 6; i++) {
            int nx = node.x + neighbors[i][0];
            int ny = node.y + neighbors[i][1];
            int nz = node.z + neighbors[i][2];

            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                BlockType block = chunk.blocks[nx][ny][nz];
                bool solid = (block != BlockType::AIR && block != BlockType::LEAVES && block != BlockType::SNOW_LEAVES && block != BlockType::TORCH && block != BlockType::GLOWSTONE);

                if (!solid) {
                    int currentSun = (chunk.light[nx][ny][nz] >> 4) & 0xF;
                    if (currentSun < node.val - 1) {
                        int newSun = node.val - 1;
                        // Write back sun (preserve existing torch)
                        int existingTorch = chunk.light[nx][ny][nz] & 0xF;
                        chunk.light[nx][ny][nz] = (unsigned char)((newSun << 4) | existingTorch);
                        sunQueue.push({ nx, ny, nz, newSun });
                    }
                }
            }
        }
    }

    // SPREAD TORCHLIGHT (BFS)
    while (!torchQueue.empty()) {
        LightNode node = torchQueue.front();
        torchQueue.pop();

        int neighbors[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

        for (int i = 0; i < 6; i++) {
            int nx = node.x + neighbors[i][0];
            int ny = node.y + neighbors[i][1];
            int nz = node.z + neighbors[i][2];

            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                BlockType block = chunk.blocks[nx][ny][nz];
                bool solid = (block != BlockType::AIR && block != BlockType::LEAVES && block != BlockType::SNOW_LEAVES && block != BlockType::TORCH && block != BlockType::GLOWSTONE);

                if (!solid) {
                    int currentTorch = chunk.light[nx][ny][nz] & 0xF;
                    if (currentTorch < node.val - 1) {
                        int newTorch = node.val - 1;
                        // Write back torch (preserve existing sun)
                        int existingSun = chunk.light[nx][ny][nz] & 0xF0;
                        chunk.light[nx][ny][nz] = (unsigned char)(existingSun | newTorch);
                        torchQueue.push({ nx, ny, nz, newTorch });
                    }
                }
            }
        }
    }
}

// Memory Pool
static std::vector<float> poolVertices[(int)BlockType::COUNT];
static std::vector<float> poolTexcoords[(int)BlockType::COUNT];
static std::vector<unsigned char> poolColors[(int)BlockType::COUNT];

/**
 * generates mesh data for a chunk using neighbor caching and memory pooling
 */
void ChunkManager::BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures) {
    UnloadChunkModels(chunk);

    // 1. neighbor caching
    Chunk* neighbors[3][3];
    for (int nx = -1; nx <= 1; nx++) {
        for (int nz = -1; nz <= 1; nz++) {
            ChunkCoord coord = { cx + nx, cz + nz };
            auto it = chunks.find(coord);
            if (it != chunks.end()) {
                neighbors[nx + 1][nz + 1] = &it->second;
            } else {
                neighbors[nx + 1][nz + 1] = nullptr;
            }
        }
    }

    // 2. clear pools
    for (int i = 0; i < (int)BlockType::COUNT; i++) {
        poolVertices[i].clear();
        poolTexcoords[i].clear();
        poolColors[i].clear();
    }

    auto getLightFast = [&](int localX, int localY, int localZ) -> int {
        // handle y out of bounds
        if (localY < 0) return 0;
        if (localY >= CHUNK_SIZE) return 15;

        // handle x/z out of bounds
        int nx = 1;
        int nz = 1;
        int lx = localX;
        int lz = localZ;

        if (localX < 0) { nx = 0; lx += CHUNK_SIZE; }
        else if (localX >= CHUNK_SIZE) { nx = 2; lx -= CHUNK_SIZE; }
        
        if (localZ < 0) { nz = 0; lz += CHUNK_SIZE; }
        else if (localZ >= CHUNK_SIZE) { nz = 2; lz -= CHUNK_SIZE; }

        Chunk* c = neighbors[nx][nz];
        if (c) return (int)c->light[lx][localY][lz];
        return 0;
    };

    auto getBlockFast = [&](int localX, int localY, int localZ) -> BlockType {
        if (localY < 0 || localY >= CHUNK_SIZE) return BlockType::AIR;
        
        int nx = 1;
        int nz = 1;
        int lx = localX;
        int lz = localZ;

        if (localX < 0) { nx = 0; lx += CHUNK_SIZE; }
        else if (localX >= CHUNK_SIZE) { nx = 2; lx -= CHUNK_SIZE; }

        if (localZ < 0) { nz = 0; lz += CHUNK_SIZE; }
        else if (localZ >= CHUNK_SIZE) { nz = 2; lz -= CHUNK_SIZE; }
        
        Chunk* c = neighbors[nx][nz];
        if (c) return c->blocks[lx][localY][lz];
        return BlockType::AIR;
    };

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType blockID = chunk.blocks[x][y][z];
                if (blockID == BlockType::AIR) continue;

                // culling checks
                bool top = (y == CHUNK_SIZE - 1) || (getBlockFast(x, y + 1, z) == BlockType::AIR);
                bool bottom = (y > 0) && (getBlockFast(x, y - 1, z) == BlockType::AIR);
                bool left = (getBlockFast(x - 1, y, z) == BlockType::AIR);
                bool right = (getBlockFast(x + 1, y, z) == BlockType::AIR);
                bool front = (getBlockFast(x, y, z + 1) == BlockType::AIR);
                bool back = (getBlockFast(x, y, z - 1) == BlockType::AIR);

                if (!top && !bottom && !left && !right && !front && !back) continue;

                float gx = (float)(cx * CHUNK_SIZE + x);
                float gy = (float)y;
                float gz = (float)(cz * CHUNK_SIZE + z);

                auto pushFace = [&](int renderID, float* vData, float* uvData, int lightLevel) {

                    // decode light
                    int sun = (lightLevel >> 4) & 0xF;
                    int torch = lightLevel & 0xF;

                    // convert to byte
                    int r = (int)((float)sun / 15.0f * 255.0f);
                    int g = (int)((float)torch / 15.0f * 255.0f);

                    Color c = { (unsigned char)r, (unsigned char)g, 0, 255 };

                    for (int k = 0; k < 18; k++) poolVertices[renderID].push_back(vData[k]);
                    for (int k = 0; k < 12; k++) poolTexcoords[renderID].push_back(uvData[k]);
                    for (int k = 0; k < 6; k++) {
                        poolColors[renderID].push_back(c.r);
                        poolColors[renderID].push_back(c.g);
                        poolColors[renderID].push_back(c.b);
                        poolColors[renderID].push_back(c.a);
                    }
                };

                auto getRenderID = [&](bool isTop, bool isBottom) -> int {
                    if (blockID == BlockType::GRASS) {
                        if (isTop) return (int)BlockType::GRASS;
                        if (isBottom) return (int)BlockType::DIRT;
                        return (int)BlockType::GRASS_SIDE;
                    }
                    if (blockID == BlockType::SNOW) {
                        if (isTop) return (int)BlockType::SNOW;
                        if (isBottom) return (int)BlockType::DIRT;
                        return (int)BlockType::SNOW_SIDE;
                    }
                    if (blockID == BlockType::SNOW_LEAVES) {
                        if (isTop) return (int)BlockType::SNOW;
                        if (isBottom) return (int)BlockType::LEAVES;
                        return (int)BlockType::SNOW_LEAVES;
                    }
                    return (int)blockID;
                };

                float fFront[] = { gx, gy, gz + 1, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy + 1, gz + 1 };
                float uvFront[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fBack[] = { gx + 1, gy, gz, gx, gy, gz, gx, gy + 1, gz, gx + 1, gy, gz, gx, gy + 1, gz, gx + 1, gy + 1, gz };
                float uvBack[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fTop[] = { gx, gy + 1, gz + 1, gx + 1, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz };
                float uvTop[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fBottom[] = { gx, gy, gz, gx + 1, gy, gz, gx, gy, gz + 1, gx, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy, gz + 1 };
                float uvBottom[] = { 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0 }; 
                float fRight[] = { gx + 1, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy + 1, gz, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz, gx + 1, gy + 1, gz + 1 };
                float uvRight[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fLeft[] = { gx, gy, gz, gx, gy, gz + 1, gx, gy + 1, gz + 1, gx, gy, gz, gx, gy + 1, gz + 1, gx, gy + 1, gz };
                float uvLeft[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                if (front)  pushFace(getRenderID(false, false), fFront, uvFront, getLightFast(x, y, z + 1));
                if (back)   pushFace(getRenderID(false, false), fBack, uvBack, getLightFast(x, y, z - 1));
                if (top)    pushFace(getRenderID(true, false), fTop, uvTop, getLightFast(x, y + 1, z));
                if (bottom) pushFace(getRenderID(false, true), fBottom, uvBottom, getLightFast(x, y - 1, z));
                if (right)  pushFace(getRenderID(false, false), fRight, uvRight, getLightFast(x + 1, y, z));
                if (left)   pushFace(getRenderID(false, false), fLeft, uvLeft, getLightFast(x - 1, y, z));
            }
        }
    }

    for (int i = 1; i < (int)BlockType::COUNT; i++) {
        if (poolVertices[i].empty()) continue;
        Mesh mesh = { 0 };
        mesh.vertexCount = (int)poolVertices[i].size() / 3;
        mesh.triangleCount = mesh.vertexCount / 3;
        mesh.vertices = (float*)MemAlloc((unsigned int)(poolVertices[i].size() * sizeof(float)));
        mesh.texcoords = (float*)MemAlloc((unsigned int)(poolTexcoords[i].size() * sizeof(float)));
        mesh.colors = (unsigned char*)MemAlloc((unsigned int)(poolColors[i].size() * sizeof(unsigned char)));
        memcpy(mesh.vertices, poolVertices[i].data(), poolVertices[i].size() * sizeof(float));
        memcpy(mesh.texcoords, poolTexcoords[i].data(), poolTexcoords[i].size() * sizeof(float));
        memcpy(mesh.colors, poolColors[i].data(), poolColors[i].size() * sizeof(unsigned char));
        UploadMesh(&mesh, false);
        chunk.layers[i] = LoadModelFromMesh(mesh);
        
        // safety check for nullptr textures if not loaded yet
        if (textures) {
            chunk.layers[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textures[i];
        }
    }
    chunk.meshReady = true;
}

void ChunkManager::UpdateAndDraw(Vector3 playerPos, Texture2D* textures, Shader shader, Color tint) {
    int playerCX = (int)floor(playerPos.x / CHUNK_SIZE);
    int playerCZ = (int)floor(playerPos.z / CHUNK_SIZE);

    for (int cx = playerCX - RENDER_DISTANCE; cx <= playerCX + RENDER_DISTANCE; cx++) {
        for (int cz = playerCZ - RENDER_DISTANCE; cz <= playerCZ + RENDER_DISTANCE; cz++) {
            ChunkCoord coord = { cx, cz };
            if (chunks.find(coord) == chunks.end()) {
                GenerateChunk(chunks[coord], cx, cz);
            }
            Chunk& chunk = chunks[coord];
            if (!chunk.meshReady) {
                BuildChunkMesh(chunk, cx, cz, textures);
            }
            for (int i = 1; i < (int)BlockType::COUNT; i++) {
                if (chunk.layers[i].meshCount > 0) {
                    chunk.layers[i].materials[0].shader = shader;
                    DrawModel(chunk.layers[i], { 0,0,0 }, 1.0f, tint);
                }
            }
        }
    }
}

/**
 * updates cellular automata processes (e.g. sand falling)
 */
void ChunkManager::UpdateChunkPhysics() {
    for (auto& pair : chunks) {
        Chunk& chunk = pair.second;

        // sleep check
        if (!chunk.shouldStep) continue;

        bool moved = false;

        // scan loops
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {

                    if (chunk.blocks[x][y][z] == BlockType::SAND) {
                        if (y > 0) {
                            if (chunk.blocks[x][y - 1][z] == BlockType::AIR) {
                                // swap
                                chunk.blocks[x][y - 1][z] = BlockType::SAND;
                                chunk.blocks[x][y][z] = BlockType::AIR;
                                moved = true;
                            }
                        }
                    }
                }
            }
        }

        if (moved) {
            chunk.meshReady = false;
            // keep awake
            chunk.shouldStep = true;
        }
        else {
            // go to sleep
            chunk.shouldStep = false;
        }
    }
}

int ChunkManager::GetLightLevel(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE) return 15; // Sky is 15

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    // If chunk doesnt exist, return 15 (Sun) or 0 (Darkness)
    // returning 0 is safer for preventing underground grid lines
    if (chunks.find(coord) == chunks.end()) return 0;

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    return (int)chunks[coord].light[lx][y][lz];
}

void ChunkManager::SaveChunks(std::ofstream& out) {
    // write how many chunks we have
    size_t count = chunks.size();
    out.write((char*)&count, sizeof(size_t));

    // loop through all chunks
    for (const auto& pair : chunks) {
        // write the Coordinate (x, z)
        out.write((char*)&pair.first, sizeof(ChunkCoord));

        // write the Raw Block Data (32x32x32 ints)
        out.write((char*)pair.second.blocks, sizeof(pair.second.blocks));

        // write the Raw Light Data (32x32x32 bytes)
        out.write((char*)pair.second.light, sizeof(pair.second.light));
    }
}

void ChunkManager::LoadChunks(std::ifstream& in) {
    // clear the current world
    UnloadAll();

    // read how many chunks to load
    size_t count;
    in.read((char*)&count, sizeof(size_t));

    // loop and recreate them
    for (size_t i = 0; i < count; i++) {
        ChunkCoord coord;
        in.read((char*)&coord, sizeof(ChunkCoord));

        // create the chunk in the map
        Chunk& chunk = chunks[coord];

        // read the Raw Data back into memory
        in.read((char*)chunk.blocks, sizeof(chunk.blocks));
        in.read((char*)chunk.light, sizeof(chunk.light));

        // flag it to be rebuilt by the renderer
        chunk.meshReady = false;
        chunk.shouldStep = true;
    }
}

void ChunkManager::RebuildMesh(int cx, int cz, Texture2D* textures) {
    ChunkCoord coord = { cx, cz };
    if (chunks.find(coord) != chunks.end()) {
        Chunk& chunk = chunks[coord];
        // only build if needed
        if (!chunk.meshReady) {
            BuildChunkMesh(chunk, cx, cz, textures);
        }
    }
}