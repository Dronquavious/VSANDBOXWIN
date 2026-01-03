#include "game.h"
#include "../world/world_generator.h"
#include <fstream>
#include <cstring>

#include "raygui.h"

void Game::Init() {
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

	world.Init();
	player.Init();
	renderer.Init();
}

void Game::Update() {

	switch (currentState) {
	case STATE_MENU:
		UpdateMenu();
		break;

	case STATE_LOADING:
		UpdateLoading();
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
			if (IsKeyPressed(KEY_P)) SaveMap();
			if (IsKeyPressed(KEY_L)) LoadMap();

			player.Update(GetFrameTime(), world);
			player.UpdateRaycast(world);
			player.HandleInput(world);
		}

		if (IsKeyPressed(KEY_P)) SaveMap(); // manual Save

		// Auto-Save (Every 60 seconds)
		autoSaveTimer += GetFrameTime();
		if (autoSaveTimer > 60.0f) {
			autoSaveTimer = 0.0f;
			SaveMap();
			messageText = "AUTO SAVED";
			messageTimer = 2.0f;
		}
		break;
	}
}

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
}

void Game::ShutDown() {
	renderer.Unload();
	world.UnloadAll();
}

void Game::SaveMap() {
	// Open file in Binary Mode
	std::ofstream out("worlds/savegame.vxl", std::ios::binary);
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

bool Game::LoadMap() {
	std::ifstream in("worlds/savegame.vxl", std::ios::binary);

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

	int sw = GetScreenWidth();
	int sh = GetScreenHeight();
	int cx = sw / 2;

	DrawText("VOXEL ENGINE", cx - MeasureText("VOXEL ENGINE", 40) / 2, 100, 40, DARKGRAY);

	// NEW GAME BUTTON
	Rectangle btnNew = { (float)cx - 100, 250, 200, 50 };
	if (GuiButton(btnNew, "NEW WORLD")) {
		world.UnloadAll();       // clear old memory
		WorldGenerator::worldSeed = GetRandomValue(0, 999999);
		player.Init();           // reset player pos
		isNewGame = true;
		currentState = STATE_LOADING; // go to loading screen
		loadingProgress = 0;
	}

	Rectangle btnLoad = { (float)cx - 100, 320, 200, 50 };
	if (GuiButton(btnLoad, "LOAD SAVED WORLD")) {

		// only switch to Loading Screen if the file actually loaded!
		if (LoadMap()) {
			isNewGame = false;
			currentState = STATE_LOADING; // Triggers the visual loading bar
			loadingProgress = 0;
		}
	}

	// RENDER DISTANCE SETTING
	// create a temporary float for the slider to use
	float fDist = (float)RENDER_DISTANCE;
	char distText[32];
	sprintf(distText, "Render Distance: %d", RENDER_DISTANCE);
	// Pass the float variable to the slider
	if (GuiSliderBar({ (float)cx - 100, 400, 200, 20 }, "Low", "High", &fDist, 4, 32)) {
		// Only update if changed
		RENDER_DISTANCE = (int)fDist;
	}

	DrawText(distText, cx - MeasureText(distText, 20) / 2, 380, 20, DARKGRAY);

	EndDrawing();
}

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