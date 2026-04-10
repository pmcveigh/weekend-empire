# Weekend Empire - Milestone 2 SDL Platform Skeleton

Weekend Empire is a football chairman simulator project. This milestone keeps the scope intentionally small and focuses on making SDL2 the first real platform layer foundation for Linux.

Milestone 1 proved a basic proof-of-life app. Milestone 2 turns that into a cleaner small skeleton that is still simple, readable, and easy to extend.

## What changed in this milestone

- tightened startup flow with explicit SDL init, window creation, OpenGL context creation, and startup logs
- improved runtime loop structure around event polling, input handling, rendering, and timing
- added cleaner key handling for both key down and key up
- Escape now quits reliably on key press
- Space and F1 key state changes are logged
- added frame timing using SDL high-resolution performance counters
- added low-noise periodic timing log output
- added simple pacing guard with `SDL_Delay(1)` for very short frames
- added useful window event logs (resize and focus changes)
- kept OpenGL use minimal (viewport + clear + swap)

## What SDL is doing for us now

At this stage, SDL is the platform layer handling:

- platform initialization (`SDL_Init`)
- native window creation
- OpenGL context creation and swap interval setup
- event queue polling
- keyboard input events
- window events (close, resize, focus)
- high-resolution frame timing
- controlled shutdown and cleanup

This gives us a solid base to build game logic later without introducing complicated architecture early.

## Current runtime behaviour

When you run the app, expected behaviour is:

- terminal shows startup stages
- a `1280x720` resizable window titled `Weekend Empire` opens
- app enters the main loop and clears the window every frame
- periodic timing logs appear approximately once per second
- keyboard logs include key down/up for `Escape`, `Space`, and `F1`
- pressing `Escape` exits cleanly
- closing the window exits cleanly
- shutdown messages confirm cleanup path

## Controls currently implemented

- `Escape`:
  - key down/up logged
  - key down triggers quit
- `Space`:
  - key down/up logged
- `F1`:
  - key down/up logged

## Dependencies (Linux, Ubuntu/Debian)

Install required packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev
```

Optional diagnostics tools that can help debugging:

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

Run the executable:

```bash
./build/weekend_empire
```

## Debugging basic failures

If configuration fails:

- ensure `libsdl2-dev` and `libgl1-mesa-dev` are installed
- confirm `cmake --version` works
- confirm compiler is available (`g++ --version`)

If window/context creation fails:

- check terminal output for `[error]` lines
- verify OpenGL support with:

```bash
glxinfo | head -n 20
```

If app runs but no window appears (remote/headless setups):

- verify an active desktop session and working display server
- if using Wayland/X11 forwarding, confirm display environment variables are valid

If runtime behaviour seems wrong:

- read startup/event/timing/shutdown logs in terminal
- run under debugger:

```bash
gdb --args ./build/weekend_empire
```

- run memory checks:

```bash
valgrind ./build/weekend_empire
```

## What this milestone proves for the Weekend Empire foundation

This milestone proves the project now has a practical, minimal platform skeleton that can support future systems safely:

- deterministic startup and shutdown path
- clear event/input loop structure
- per-frame timing data available for future simulation updates
- stable rendering loop with minimal OpenGL dependency
- runtime logging that helps diagnose platform issues early

It is still deliberately small and procedural, but no longer just proof-of-life. It is now a clean base for the next feature milestone.
