#include "game.h"
#include "../world/world_generator.h"
#include <fstream>
#include <cstring>

#include "raygui.h"

/**
 * initializes game systems, defaults, and states
 */
void Game::Init() {
	SetExitKey(0); // disables default esc behavior

	// defaults
	daySpeed = 0.005f;
	timeOfDay = 0.0f;
	timeMode = 0;
	showDebugUI = false;
	messageTimer = 0.0f;
	messageText = "";
	physicsTimer = 0.0f;


	DisableCursor();
	HideCursor();

	// start in Menu
	currentState = STATE_MENU;

	// init auto-save
	autoSaveTimer = 0.0f;

	// other defaults
	strcpy(worldNameBuffer, "New World");
	strcpy(seedBuffer, "12345");
	selectedSaveIndex = -1;
	currentSaveName = "savegame.vxl";
	editWorldNameMode = false;
	editSeedMode = false;

	world.Init();
	player.Init();
	renderer.Init();
}

/**
 * main update loop
 * delegates to sub-update functions based on state
 */
void Game::Update() {

	switch (currentState) {
	case STATE_MENU:
		UpdateMenu();
		break;

	case STATE_LOADING:
		UpdateLoading();
		break;

	case STATE_PAUSE:
		if (IsKeyPressed(KEY_ESCAPE)) {
			DisableCursor(); // lock mouse again
			currentState = STATE_PLAYING;
		}
		break;

	case STATE_PLAYING:

		physicsTimer += GetFrameTime();
		if (physicsTimer >= 0.05f) {
			physicsTimer -= 0.05f;
			world.UpdateChunkPhysics();
		}

		// time of day logic
		if (timeMode == 0) { // cycle
			timeOfDay += GetFrameTime() * daySpeed;
			if (timeOfDay > 1.0f) timeOfDay = 0.0f;
		}
		else if (timeMode == 1) { // always day
			timeOfDay = 0.5f;
		}
		else if (timeMode == 2) { // always night
			timeOfDay = 0.0f;
		}

		if (messageTimer > 0.0f) messageTimer -= GetFrameTime();

		if (IsKeyPressed(KEY_TAB)) {
			showDebugUI = !showDebugUI;
			if (showDebugUI) EnableCursor();
			else DisableCursor();
		}

		if (!showDebugUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			DisableCursor();
		}

		if (showDebugUI) {
			// menu mode
		}
		else {
			if (IsKeyPressed(KEY_P)) SaveMap(currentSaveName.c_str());
			if (IsKeyPressed(KEY_L)) LoadMap(currentSaveName.c_str());

			player.Update(GetFrameTime(), world);
			player.UpdateRaycast(world);
			player.HandleInput(world);
		}

		if (IsKeyPressed(KEY_P)) SaveMap(currentSaveName.c_str()); // manual Save

		// Auto-Save (Every 60 seconds)
		autoSaveTimer += GetFrameTime();
		if (autoSaveTimer > 60.0f) {
			autoSaveTimer = 0.0f;
			SaveMap(currentSaveName.c_str());
			messageText = "AUTO SAVED";
			messageTimer = 2.0f;
		}

		if (IsKeyPressed(KEY_ESCAPE)) {
			EnableCursor(); // unlock mouse so we can click buttons
			currentState = STATE_PAUSE;
		}

		break;
	}
}

/**
 * main draw loop
 * delegates to renderer and sub-draw functions
 */
void Game::Draw() {
	if (currentState == STATE_PLAYING) {
		renderer.DrawScene(player, world, timeOfDay);
		renderer.DrawUI(player, GetScreenWidth(), GetScreenHeight(), messageText, messageTimer);
		if (showDebugUI) {
			renderer.DrawDebug(player, daySpeed, timeMode);
		}
		EndDrawing();
	}
	else if (currentState == STATE_MENU) {
		DrawMenu();
	}
	else if (currentState == STATE_LOADING) {
		DrawLoading();
	}
	else if (currentState == STATE_PAUSE) {
		// draw the game behind it (frozen)
		renderer.DrawScene(player, world, timeOfDay);

		// semi-transparent black overlay
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.4f));

		// draw Menu
		int cx = GetScreenWidth() / 2;
		int cy = GetScreenHeight() / 2;

		DrawText("PAUSED", cx - MeasureText("PAUSED", 40) / 2, cy - 100, 40, WHITE);

		// RESUME
		if (GuiButton({ (float)cx - 100, (float)cy, 200, 50 }, "RESUME")) {
			DisableCursor();
			currentState = STATE_PLAYING;
		}

		// SAVE & QUIT
		if (GuiButton({ (float)cx - 100, (float)cy + 70, 200, 50 }, "SAVE & QUIT")) {
			SaveMap(currentSaveName.c_str()); // save before leaving
			world.UnloadAll(); // free memory
			currentState = STATE_MENU;
		}
		EndDrawing();
	}
}

void Game::ShutDown() {
	renderer.Unload();
	world.UnloadAll();
}

void Game::SaveMap(const char* filename) {
	// Open file in Binary Mode
	if (!DirectoryExists("worlds")) MakeDirectory("worlds");

	std::string path = "worlds/";
	path += filename;

	std::ofstream out(path, std::ios::binary);
	if (!out) {
		messageText = "FAILED TO SAVE GAME";
		messageTimer = 3.0f;
		return;
	}

	// HEADER (Magic Number + Version)
	const char* magic = "VOXL";
	int version = 1;
	out.write(magic, 4);
	out.write((char*)&version, sizeof(int));

	// WORLD GLOBAL DATA
	out.write((char*)&WorldGenerator::worldSeed, sizeof(int));

	// PLAYER DATA (Position, Camera, Inventory)
	// Since Inventory is a simple struct (POD), we can write it directly
	out.write((char*)&player.position, sizeof(Vector3));
	out.write((char*)&player.cameraAngleX, sizeof(float));
	out.write((char*)&player.cameraAngleY, sizeof(float));
	out.write((char*)&player.inventory, sizeof(Inventory));

	// CHUNK DATA
	world.SaveChunks(out);

	out.close();
	messageText = "GAME SAVED!";
	messageTimer = 2.0f;
}

/**
 * loads world data from binary file
 */
bool Game::LoadMap(const char* filename) {
	
	if (!DirectoryExists("worlds")) MakeDirectory("worlds");
	std::string path = "worlds/";
	path += filename;

	std::ifstream in(path, std::ios::binary);

	if (!in) {
		messageText = "NO SAVE FILE FOUND";
		messageTimer = 3.0f;
		return false;
	}

	// HEADER CHECK
	char magic[5] = { 0 };
	in.read(magic, 4);
	if (strcmp(magic, "VOXL") != 0) {
		messageText = "INVALID SAVE FILE";
		messageTimer = 3.0f;
		return false;
	}

	int version;
	in.read((char*)&version, sizeof(int));

	// WORLD GLOBAL DATA
	in.read((char*)&WorldGenerator::worldSeed, sizeof(int));

	// PLAYER DATA
	in.read((char*)&player.position, sizeof(Vector3));
	in.read((char*)&player.cameraAngleX, sizeof(float));
	in.read((char*)&player.cameraAngleY, sizeof(float));
	in.read((char*)&player.inventory, sizeof(Inventory));

	// Recalculate vectors
	player.forward = { sinf(player.cameraAngleX), 0.0f, cosf(player.cameraAngleX) };
	player.right = { cosf(player.cameraAngleX), 0.0f, -sinf(player.cameraAngleX) };

	// CHUNK DATA
	// This reads the blocks but DOES NOT build the meshes yet (fast!)
	world.LoadChunks(in);

	in.close();
	messageText = "GAME LOADED!";
	messageTimer = 2.0f;

	return true; // <--- Success!
}

void Game::UpdateMenu() {
	ShowCursor(); // mouse must be seen
}

void Game::DrawMenu() {
	BeginDrawing();
	ClearBackground(RAYWHITE);
	int cx = GetScreenWidth() / 2;

	DrawText("VOXEL ENGINE", cx - 150, 50, 40, DARKGRAY);

	// --- LEFT SIDE: CREATE WORLD ---
	GuiGroupBox({ (float)cx - 320, 150, 300, 300 }, "CREATE NEW");

	// WORLD NAME INPUT
	GuiLabel({ (float)cx - 300, 180, 100, 20 }, "World Name:");

	if (GuiTextBox({ (float)cx - 300, 200, 260, 30 }, worldNameBuffer, 64, editWorldNameMode)) {
		// toggle mode when clicked
		editWorldNameMode = !editWorldNameMode;
	}

	// SEED INPUT
	GuiLabel({ (float)cx - 300, 240, 100, 20 }, "Seed (Number):");

	if (GuiTextBox({ (float)cx - 300, 260, 260, 30 }, seedBuffer, 64, editSeedMode)) {
		// toggle mode when clicked
		editSeedMode = !editSeedMode;
	}

	// create Button
	if (GuiButton({ (float)cx - 300, 380, 260, 40 }, "CREATE WORLD")) {
		world.UnloadAll();
		WorldGenerator::worldSeed = atoi(seedBuffer);
		player.Init();
		isNewGame = true;

		currentSaveName = std::string(worldNameBuffer) + ".vxl";

		// optional: Save immediately so the file exists
		SaveMap(currentSaveName.c_str());

		currentState = STATE_LOADING;
		loadingProgress = 0;
	}

	// --- RIGHT SIDE: LOAD WORLD ---
	GuiGroupBox({ (float)cx + 20, 150, 300, 300 }, "LOAD WORLD");

	// Refresh List
	if (GuiButton({ (float)cx + 40, 170, 260, 30 }, "REFRESH LIST")) {
		saveFiles.clear();
		FilePathList files = LoadDirectoryFiles("worlds");
		for (int i = 0; i < files.count; i++) {
			if (IsFileExtension(files.paths[i], ".vxl")) {
				saveFiles.push_back(GetFileName(files.paths[i]));
			}
		}
		UnloadDirectoryFiles(files);
	}

	// sraw List
	int y = 210;
	for (int i = 0; i < saveFiles.size(); i++) {
		// truncate name if too long for button
		if (GuiButton({ (float)cx + 40, (float)y, 260, 30 }, saveFiles[i].c_str())) {

			// update currentSaveName
			currentSaveName = saveFiles[i];

			if (LoadMap(currentSaveName.c_str())) {
				isNewGame = false;
				currentState = STATE_LOADING;
				loadingProgress = 0;
			}
		}
		y += 35;
	}

	EndDrawing();
}

/**
 * handles partial chunk loading during loading screen
 */
void Game::UpdateLoading() {
	// Goal: Pre-load the chunks around the player before starting.
	int loadRadius = 6;
	static int currentX = -loadRadius;
	static int currentZ = -loadRadius;

	// RESET LOOP (Start Condition)
	if (loadingProgress == 0) {
		currentX = -loadRadius;
		currentZ = -loadRadius;
		loadingProgress = 1;
	}

	// generate specific chunk
	int px = (int)floor(player.position.x / CHUNK_SIZE);
	int pz = (int)floor(player.position.z / CHUNK_SIZE);

	int cx = px + currentX;
	int cz = pz + currentZ;

	// Force Generation
	world.GetBlock(cx * CHUNK_SIZE, 0, cz * CHUNK_SIZE);

	// need the textures to build the mesh
	Texture2D* tex = renderer.GetTextures();
	world.RebuildMesh(cx, cz, tex);

	// move to next chunk
	currentZ++;
	if (currentZ > loadRadius) {
		currentZ = -loadRadius;
		currentX++;
	}

	// calculate Progress %
	int totalChunks = (loadRadius * 2 + 1) * (loadRadius * 2 + 1);
	int chunksDone = (currentX + loadRadius) * (loadRadius * 2 + 1) + (currentZ + loadRadius);

	loadingProgress = (int)((float)chunksDone / (float)totalChunks * 100.0f);

	// FIX: Prevent infinite loop
	// If the math returns 0, force it to 1 so the reset logic doesn't trigger again.
	if (loadingProgress == 0) loadingProgress = 1;

	// FINISHED?
	if (currentX > loadRadius) {
		currentState = STATE_PLAYING;
		loadingProgress = 0; // Reset for next time
		DisableCursor();
	}
}

void Game::DrawLoading() {
	BeginDrawing();
	ClearBackground(BLACK);

	int sw = GetScreenWidth();
	int sh = GetScreenHeight();

	DrawText("GENERATING TERRAIN...", sw / 2 - 150, sh / 2 - 50, 30, WHITE);

	// draw Bar
	DrawRectangleLines(sw / 2 - 200, sh / 2, 400, 30, WHITE);
	DrawRectangle(sw / 2 - 195, sh / 2 + 5, (int)(3.9f * loadingProgress), 20, WHITE);

	EndDrawing();
}