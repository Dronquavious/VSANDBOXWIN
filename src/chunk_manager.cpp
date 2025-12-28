#include "chunk_manager.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <vector>

// --- HELPER: SIMPLE 3D NOISE FOR CAVES ---
// Standard "Value Noise" to create smooth, swiss-cheese caves.

float Fract(float x) { return x - floorf(x); }

float Hash3D(int x, int y, int z) {
    // scramble bits to get a pseudo-random number between 0.0 and 1.0
    int h = x * 374761393 + y * 668265263 + z * 924083321;
    h = (h ^ (h >> 13)) * 1274126177;
    return (h & 0xFFFF) / 65535.0f;
}

float SimpleNoise3D(float x, float y, float z) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);

    float fx = Fract(x);
    float fy = Fract(y);
    float fz = Fract(z);

    // smoothstep interpolation (makes caves round instead of blocky)
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);
    float w = fz * fz * (3.0f - 2.0f * fz);

    float c000 = Hash3D(ix, iy, iz);
    float c100 = Hash3D(ix + 1, iy, iz);
    float c010 = Hash3D(ix, iy + 1, iz);
    float c110 = Hash3D(ix + 1, iy + 1, iz);
    float c001 = Hash3D(ix, iy, iz + 1);
    float c101 = Hash3D(ix + 1, iy, iz + 1);
    float c011 = Hash3D(ix, iy + 1, iz + 1);
    float c111 = Hash3D(ix + 1, iy + 1, iz + 1);

    float x1 = Lerp(c000, c100, u);
    float x2 = Lerp(c010, c110, u);
    float y1 = Lerp(x1, x2, v);

    float x3 = Lerp(c001, c101, u);
    float x4 = Lerp(c011, c111, u);
    float y2 = Lerp(x3, x4, v);

    return Lerp(y1, y2, w);
}

void ChunkManager::Init() {}

void ChunkManager::UnloadAll() {
    for (auto& pair : chunks) {
        UnloadChunkModels(pair.second);
    }
    chunks.clear();
}

void ChunkManager::UnloadChunkModels(Chunk& chunk) {
    for (int i = 0; i < 9; i++) {
        if (chunk.layers[i].meshCount > 0) {
            UnloadModel(chunk.layers[i]);
            chunk.layers[i] = { 0 };
        }
    }
    chunk.meshReady = false;
}

int ChunkManager::GetBlock(int x, int y, int z, bool createIfMissing) {
    if (y < 0 || y >= CHUNK_SIZE) return 0;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) {
        if (createIfMissing) GenerateChunk(chunks[coord], cx, cz);
        else return 0;
    }

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    return chunks[coord].blocks[lx][y][lz];
}

void ChunkManager::SetBlock(int x, int y, int z, int type) {
    if (y < 0 || y >= CHUNK_SIZE) return;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) GenerateChunk(chunks[coord], cx, cz);

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    chunks[coord].blocks[lx][y][lz] = type;
    chunks[coord].meshReady = false;
}

bool ChunkManager::IsBlockSolid(int x, int y, int z) {
    return GetBlock(x, y, z) != 0;
}

// --- MESH GENERATION ---
void ChunkManager::BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures) {
    UnloadChunkModels(chunk);

    std::vector<float> vertices[9];
    std::vector<float> texcoords[9];

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {

                int blockID = chunk.blocks[x][y][z];
                if (blockID == 0) continue;

                // CULLING
                bool top = (y == CHUNK_SIZE - 1) || (chunk.blocks[x][y + 1][z] == 0);
                bool bottom = (y > 0) && (chunk.blocks[x][y - 1][z] == 0);
                bool left = (x == 0) || (chunk.blocks[x - 1][y][z] == 0);
                bool right = (x == CHUNK_SIZE - 1) || (chunk.blocks[x + 1][y][z] == 0);
                bool front = (z == CHUNK_SIZE - 1) || (chunk.blocks[x][y][z + 1] == 0);
                bool back = (z == 0) || (chunk.blocks[x][y][z - 1] == 0);

                if (!top && !bottom && !left && !right && !front && !back) continue;

                float gx = (float)(cx * CHUNK_SIZE + x);
                float gy = (float)y;
                float gz = (float)(cz * CHUNK_SIZE + z);

                auto pushFace = [&](int renderID, float* vData, float* uvData) {
                    for (int k = 0; k < 18; k++) vertices[renderID].push_back(vData[k]);
                    for (int k = 0; k < 12; k++) texcoords[renderID].push_back(uvData[k]);
                    };

                auto getRenderID = [&](bool isTop, bool isBottom) {
                    if (blockID == 4) {
                        if (isTop) return 4;
                        if (isBottom) return 1;
                        return 8;
                    }
                    return blockID;
                    };

                // --- FIXED VERTEX DATA ---

                // Front (Z+)
                float fFront[] = { gx, gy, gz + 1, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy + 1, gz + 1 };
                float uvFront[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                // Back (Z-)
                float fBack[] = { gx + 1, gy, gz, gx, gy, gz, gx, gy + 1, gz, gx + 1, gy, gz, gx, gy + 1, gz, gx + 1, gy + 1, gz };
                float uvBack[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                // Top (Y+)
                float fTop[] = { gx, gy + 1, gz + 1, gx + 1, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz };
                float uvTop[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                // Bottom (Y-) -- FIXED GEOMETRY --
                float fBottom[] = { gx, gy, gz, gx + 1, gy, gz, gx, gy, gz + 1,   gx, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy, gz + 1 };
                float uvBottom[] = { 0, 1, 1, 1, 0, 0,   0, 0, 1, 1, 1, 0 };

                // Right (X+)
                float fRight[] = { gx + 1, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy + 1, gz, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz, gx + 1, gy + 1, gz + 1 };
                float uvRight[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                // Left (X-)
                float fLeft[] = { gx, gy, gz, gx, gy, gz + 1, gx, gy + 1, gz + 1, gx, gy, gz, gx, gy + 1, gz + 1, gx, gy + 1, gz };
                float uvLeft[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                if (front)  pushFace(getRenderID(false, false), fFront, uvFront);
                if (back)   pushFace(getRenderID(false, false), fBack, uvBack);
                if (top)    pushFace(getRenderID(true, false), fTop, uvTop);
                if (bottom) pushFace(getRenderID(false, true), fBottom, uvBottom);
                if (right)  pushFace(getRenderID(false, false), fRight, uvRight);
                if (left)   pushFace(getRenderID(false, false), fLeft, uvLeft);
            }
        }
    }

    for (int i = 1; i <= 8; i++) {
        if (vertices[i].empty()) continue;

        Mesh mesh = { 0 };
        mesh.vertexCount = (int)vertices[i].size() / 3;
        mesh.triangleCount = mesh.vertexCount / 3;

        // Fix warning: explicit cast to unsigned int
        mesh.vertices = (float*)MemAlloc((unsigned int)(vertices[i].size() * sizeof(float)));
        mesh.texcoords = (float*)MemAlloc((unsigned int)(texcoords[i].size() * sizeof(float)));

        memcpy(mesh.vertices, vertices[i].data(), vertices[i].size() * sizeof(float));
        memcpy(mesh.texcoords, texcoords[i].data(), texcoords[i].size() * sizeof(float));

        UploadMesh(&mesh, false);
        chunk.layers[i] = LoadModelFromMesh(mesh);
        chunk.layers[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textures[i];
    }
    chunk.meshReady = true;
}

// --- UPDATE AND DRAW ---
void ChunkManager::UpdateAndDraw(Vector3 playerPos, Texture2D* textures) {
    int playerCX = (int)floor(playerPos.x / CHUNK_SIZE);
    int playerCZ = (int)floor(playerPos.z / CHUNK_SIZE);
    int renderDist = 4;

    for (int cx = playerCX - renderDist; cx <= playerCX + renderDist; cx++) {
        for (int cz = playerCZ - renderDist; cz <= playerCZ + renderDist; cz++) {
            ChunkCoord coord = { cx, cz };
            if (chunks.find(coord) == chunks.end()) {
                GenerateChunk(chunks[coord], cx, cz);
            }
            Chunk& chunk = chunks[coord];
            if (!chunk.meshReady) {
                BuildChunkMesh(chunk, cx, cz, textures);
            }
            for (int i = 1; i <= 8; i++) {
                if (chunk.layers[i].meshCount > 0) {
                    DrawModel(chunk.layers[i], { 0,0,0 }, 1.0f, WHITE);
                }
            }
        }
    }
}

// --- TREE GENERATION ---
void ChunkManager::CreateTree(Chunk& chunk, int x, int y, int z)
{
    int height = GetRandomValue(4, 6);

    // Trunk
    for (int i = 0; i < height; i++) {
        if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = 3;
    }

    // Leaves
    int leafStart = height - 2;
    int leafEnd = height + 1;

    for (int ly = leafStart; ly <= leafEnd; ly++) {
        int radius = (ly == leafEnd) ? 1 : 2;
        for (int lx = -radius; lx <= radius; lx++) {
            for (int lz = -radius; lz <= radius; lz++) {
                if (abs(lx) == radius && abs(lz) == radius && (ly != leafEnd)) continue;
                int finalX = x + lx;
                int finalY = y + ly;
                int finalZ = z + lz;

                if (finalX >= 0 && finalX < CHUNK_SIZE &&
                    finalY >= 0 && finalY < CHUNK_SIZE &&
                    finalZ >= 0 && finalZ < CHUNK_SIZE)
                {
                    if (chunk.blocks[finalX][finalY][finalZ] == 0) {
                        chunk.blocks[finalX][finalY][finalZ] = 7;
                    }
                }
            }
        }
    }
}

// --- GENERATION (NOW WITH CAVES) ---
void ChunkManager::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
    int offsetX = chunkX * CHUNK_SIZE;
    int offsetZ = chunkZ * CHUNK_SIZE;

    Image noiseImage = GenImagePerlinNoise(CHUNK_SIZE, CHUNK_SIZE, offsetX, offsetZ, 4.0f);
    Color* pixels = LoadImageColors(noiseImage);

    // PASS 1: Ground + Caves
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 2;

            for (int y = 0; y < CHUNK_SIZE; y++) {
                // default block at this position
                int blockType = 0;

                if (y == 0) blockType = 6; // bedrock (Never cut this)
                else if (y < height - 3) blockType = 2; // stone
                else if (y < height) blockType = 1; // dirt
                else if (y == height) {
                    if (height < 6) blockType = 5; // sand
                    else blockType = 4; // grass
                }

                // --- CAVE GENERATION ---
                // carve air out of STONE, but we leave dirt/grass intact 
                // so the surface doesn't look like swiss cheese.
                if (blockType == 2) {
                    float caveNoise = SimpleNoise3D(
                        (float)(offsetX + x) * 0.06f,
                        (float)y * 0.06f,
                        (float)(offsetZ + z) * 0.06f
                    );

                    // if noise is high, it's a cave (Air)
                    if (caveNoise > 0.6f) blockType = 0;
                }

                chunk.blocks[x][y][z] = blockType;
            }
        }
    }

    // PASS 2: Trees
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            Color c = pixels[z * CHUNK_SIZE + x];
            int height = (int)((c.r / 255.0f) * 12.0f) + 2;

            // only plant trees if the block below is actually Grass (not a cave entrance)
            if (chunk.blocks[x][height][z] == 4) {
                if (GetRandomValue(0, 100) < 1) {
                    if (x > 2 && x < CHUNK_SIZE - 3 && z > 2 && z < CHUNK_SIZE - 3) {
                        CreateTree(chunk, x, height + 1, z);
                    }
                }
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(noiseImage);
}