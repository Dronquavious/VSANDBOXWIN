# Voxel Sandbox (Windows Edition)

This is a high-performance C++ voxel engine built from scratch using **Raylib**.

## 📖 Backstory
This project is an optimized continuation of my original [Mac/Linux Voxel Sandbox]([https://github.com/Dronquavious/VSANDBOX]).

As the complexity of the engine grew (infinite chunks, procedural generation, voxel meshing), the hardware limitations on my previous machine began to bottleneck performance. This repository represents the migration to a Windows environment to leverage better hardware acceleration and Visual Studio's profiling tools.

![dev preview](assets/VSANDBOXWINPREV.gif)

## 📂 Project Structure
* `src/` - Contains all source code (`.cpp`).
* `include/` - Contains all header files (`.h`).
* `assets/` - Contains Assets
* `VSANDBOXWIN.slnx` - The Visual Studio Solution file.

## 🚀 Features
* **Infinite Terrain Generation:** Procedural world generation using Perlin noise.
* **Chunk System:** Optimized mesh building with face culling (no internal faces rendered).
* **Cave Systems:** 3D Noise generation to create underground tunnel networks.
* **Dynamic Day/Night Cycle:** With adjustable time speeds and cycle modes.
* **Building System:** Minecraft-style block placement and destruction.
* **Optimized Rendering:** Uses custom vertex buffers for high FPS on complex scenes.

## 🛠️ How to Build
1.  **Prerequisites:**
    * Visual Studio 2022 (or newer) (with "Desktop development with C++" workload).
    * **Raylib:** You must have the Raylib library installed or linked via NuGet.
2.  **Installation:**
    * Clone this repository: `git clone https://github.com/Dronquavious/VSANDBOXWIN.git`
    * Open `VSANDBOXWIN.slnx`.
3.  **Running:**
    * Select `Release` and `x64` in the top toolbar.
    * Press **F5** (Local Windows Debugger).

## 🎮 Controls
* **WASD:** Move
* **Space:** Jump / Fly Up
* **Left Ctrl:** Fly Down
* **F:** Toggle Fly Mode
* **Tab:** Open Debug/Settings Menu
* **Left Click:** Break Block
* **Right Click:** Place Block
* **1-7:** Select Block Type

## 📄 License
This project is open source and under the MIT License. Feel free to use it for learning or as a base for your own voxel engine!

## Contact

If you have questions, feedback, or want to discuss the project, you can reach me at:

- GitHub: @Dronquavious
- Email: tambweamani@gmail.com

Feel free to reach out — I’m happy to talk about the project or help you get started.