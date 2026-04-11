# Weekend Empire - Milestone 4 Dear ImGui + SQLite Foundation

Weekend Empire is a football chairman simulator project. This milestone extends the existing Linux-first C++20 SDL2 + OpenGL base by adding two practical foundation layers early:

- Dear ImGui for rapid prototype management screens
- SQLite for simple structured game data

The focus remains deliberately small, readable, and framework-first.

## What changed in this milestone

- integrated Dear ImGui into the existing SDL2 + OpenGL render loop
- added a visible in-app prototype panel titled `Weekend Empire`
- added basic chairman-style fields shown in the panel
- added `Advance Day` and `Refresh Club Data` buttons with log output
- integrated SQLite using a minimal, deterministic schema
- initialised/verified schema on startup
- inserted sample data if missing
- loaded real data from SQLite into the ImGui panel
- extended startup/runtime/shutdown logging for ImGui and SQLite lifecycle

## Why Dear ImGui is added now

Dear ImGui lets us build useful prototype UI immediately without spending time on polished frontend systems.

For this stage, that means we can:

- verify that a practical in-game management panel can render every frame
- quickly iterate on visible game-shaped information
- keep UI work low-cost and low-risk while gameplay systems are still undefined

## Why SQLite is added now

SQLite gives us a simple, local structured data backbone early.

For this stage, that means we can:

- stop relying only on hardcoded values
- start validating game-facing data flow from storage to UI
- keep persistence simple without introducing a large architecture

## What this milestone proves

This milestone proves the project can now run a small but real framework slice:

- SDL2 initialises platform and events
- OpenGL initialises rendering backend
- Dear ImGui initialises and renders a management-style prototype window
- SQLite initialises and provides structured data
- schema and sample seed data are validated on startup
- UI displays data read from SQLite
- UI interactions trigger logs and keep the app responsive
- clean shutdown still works

## Current minimal data model

The schema is intentionally tiny:

- `leagues`
  - `id`
  - `name`
- `clubs`
  - `id`
  - `name`
  - `league_id` (foreign key to `leagues.id`)
  - `balance`
  - `wage_bill`
  - `season_phase`
  - `fixture_count`

Seed values are deterministic and currently include one league and one club prototype row.

## What the app now displays

The main ImGui panel displays a small chairman-style overview loaded from SQLite:

- club name
- league name
- balance
- wage bill
- season phase
- fixture count
- last UI action text

Buttons currently provide framework behaviour:

- `Advance Day`
  - logs a UI action only
- `Refresh Club Data`
  - reloads the current club overview from SQLite and logs the action

## Real data vs framework behaviour

### Real data

- displayed club/league/finance/season fields are loaded from SQLite
- schema creation/verification is real
- sample insertion (when needed) is real
- refresh action performs a real DB read

### Framework behaviour only (placeholder at this milestone)

- `Advance Day` does not advance simulation state yet
- no match simulation, scheduling, transfer systems, or staff/player modeling
- no save-game slot management beyond the single local SQLite file
- no polished frontend layout/theming

## Dependencies (Linux, Ubuntu/Debian)

Install required packages:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev libsqlite3-dev
```

Optional diagnostics packages:

```bash
sudo apt install -y gdb valgrind mesa-utils sqlite3
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

On first run, the app creates `weekend_empire.db` in the working directory if it does not exist.

## Expected runtime behaviour

On startup, terminal logs should show stage progression similar to:

- Weekend Empire startup
- SDL init
- OpenGL init and context details
- Dear ImGui init
- SQLite init
- schema verification
- sample data verification
- data load for UI

During runtime:

- the ImGui `Weekend Empire` panel is visible
- prototype fields display database-backed values
- button presses log actions
- `Escape` still exits cleanly

On shutdown:

- SQLite closes
- ImGui backend/context shutdown occurs
- SDL/OpenGL resources are cleaned up

## Debugging guidance for common issues

### Configure/build fails while fetching Dear ImGui

- verify network access from your environment
- remove stale build cache and reconfigure:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

### Missing development packages

If CMake cannot find SDL2/OpenGL/SQLite3, install:

```bash
sudo apt install -y libsdl2-dev libgl1-mesa-dev libsqlite3-dev
```

### OpenGL context creation fails

- check terminal `[error]` logs
- verify OpenGL availability:

```bash
glxinfo | head -n 20
```

### App runs but panel is not visible

- confirm Dear ImGui init logs appear
- ensure the app is not immediately closing due to startup errors
- run from a terminal to inspect logs directly

### Database file or data inspection

Check that database file exists and inspect seeded data:

```bash
sqlite3 weekend_empire.db ".tables"
sqlite3 weekend_empire.db "SELECT * FROM leagues;"
sqlite3 weekend_empire.db "SELECT * FROM clubs;"
```

### Runtime debugging tools

Debugger:

```bash
gdb --args ./build/weekend_empire
```

Memory checker:

```bash
valgrind ./build/weekend_empire
```
