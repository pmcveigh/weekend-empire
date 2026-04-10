# Weekend Empire - Milestone 1 Bootstrap

This repository starts Weekend Empire with the smallest possible native Linux application skeleton.

This first milestone is intentionally simple. It proves the repo can build, SDL2 and OpenGL link correctly, a window opens, input/events are processed, the main loop runs, rendering works, and shutdown is clean.

## What this starter app does

- Initialises SDL2 for video/events/timer
- Creates an SDL window with an OpenGL context
- Runs a basic loop that polls SDL events
- Handles quit via window close and `Escape`
- Logs simple startup/runtime/shutdown messages
- Logs a few key presses (`Escape`, `Space`, `F1`)
- Clears the screen each frame to a visible color and swaps buffers

## Linux setup (Ubuntu/Debian)

1. Install likely required packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev
```

2. Verify tools are available:

```bash
cmake --version
g++ --version
```

## Configure, build, and run

From the repository root:

1. Configure:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

2. Build:

```bash
cmake --build build -j
```

3. Run:

```bash
./build/weekend_empire
```

## Expected runtime behavior

- A `1280x720` window titled `Weekend Empire` appears.
- The window displays a solid blue-ish background.
- Terminal logs show startup and loop messages.
- Press `Space` or `F1` to see key event logs.
- Press `Escape` or click the window close button to quit.
- The app logs shutdown and exits cleanly.

## Clean and rebuild

Delete the build directory and reconfigure:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```
