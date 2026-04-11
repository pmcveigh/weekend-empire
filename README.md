# Weekend Empire - Milestone 3 OpenGL Rendering Foundation

Weekend Empire is a football chairman simulator project. This milestone keeps the Linux-first C++20 foundation small and readable while making OpenGL a deliberate rendering backend choice.

## What changed in this milestone

- kept SDL2 as the platform layer for windowing, input, timing, events, and OpenGL context creation
- made OpenGL setup more explicit with clearer context attribute configuration and startup stage logs
- added startup logging for OpenGL vendor, renderer, and version
- kept rendering deliberately minimal: viewport + clear colour + buffer swap
- made viewport updates explicit on resize-related window events
- retained clean event loop, key handling, timing logs, and shutdown path

## Why OpenGL at this stage

This project does not need Vulkan complexity right now. OpenGL is enough to provide a reliable rendering foundation while we continue building gameplay and UI systems step by step.

Using OpenGL here gives us:

- a practical, widely supported path for Linux-first development
- simple and visible rendering behaviour that is easy to debug
- a stable base for later milestones without introducing heavy architecture early

## What this milestone proves

This milestone proves the SDL2 + OpenGL baseline is now intentionally structured rather than incidental:

- startup and context creation are explicit and validated
- OpenGL runtime details are visible in logs
- resize handling keeps rendering valid through viewport updates
- rendering loop remains minimal, stable, and understandable

## Expected runtime behaviour

When running the app:

- SDL initialises successfully
- OpenGL context is created successfully
- terminal startup logs show OpenGL vendor, renderer, and version
- window opens at `1280x720` and displays a clear background colour
- resizing the window keeps rendering valid
- `Escape` quits cleanly
- window close event quits cleanly
- shutdown logs confirm orderly cleanup

## Controls

- `Escape`
  - key down/up logged
  - key down exits application
- `Space`
  - key down/up logged
- `F1`
  - key down/up logged

## Dependencies (Linux, Ubuntu/Debian)

Install required packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev
```

Optional diagnostics tools:

```bash
sudo apt install -y gdb valgrind mesa-utils
```

## Build steps

From repository root:

1. Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

2. Build

```bash
cmake --build build -j
```

## Run steps

```bash
./build/weekend_empire
```

## Debugging guidance for common Linux issues

### Configure/build errors

- verify dependencies are installed (`libsdl2-dev`, `libgl1-mesa-dev`)
- verify toolchain availability:

```bash
cmake --version
g++ --version
```

### Window or OpenGL context creation fails

- check terminal output for `[error]` log lines
- verify OpenGL availability from your session:

```bash
glxinfo | head -n 20
```

### Startup logs show unexpected renderer/version

- this usually indicates a driver/environment mismatch
- ensure GPU drivers are installed correctly for your hardware
- on remote/virtual sessions, confirm hardware acceleration or expected software renderer behaviour

### App runs but no visible window (remote/headless)

- confirm an active desktop session and valid display environment
- verify your Wayland/X11 session is configured correctly

### Runtime behaviour debugging

Use debugger:

```bash
gdb --args ./build/weekend_empire
```

Use memory checker:

```bash
valgrind ./build/weekend_empire
```
