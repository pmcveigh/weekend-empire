# Weekend Empire - Milestone 6 Early Chairman Agency Prototype

Weekend Empire is a Linux-first C++20 football chairman simulator prototype built with SDL2, OpenGL, Dear ImGui, and SQLite.

This milestone moves the game from a passive simulation dashboard into a small but coherent chairman sandbox with meaningful decisions, league identity, board pressure, and supporter reaction context.

## What changed in this milestone

- Added a chairman decision/event layer with actionable choices and state consequences.
- Expanded the world to a 10-club league with identity fields (strength, reputation, supporter expectation, fanbase).
- Added rivalry links used by derby context and sentiment swings.
- Added visible league table standings derived from played fixtures.
- Added clearer finance categories and a richer finance ledger panel.
- Added explicit board objectives tied to table position, rival performance, form, wages, and balance floor.
- Added supporter reaction logging and daily supporter context updates.
- Extended SQLite schema for events, objectives, rivalry links, and supporter notes.

## New systems in this milestone

## 1) Decision/Event Layer

Active chairman events now appear in a dedicated decision panel.

Each event provides 2-3 choices, and each choice has explicit outcome effects shown before you click:

- balance delta
- board confidence delta
- supporter mood delta
- reputation delta

Event themes are grounded in chairman duties:

- sponsor offers
- ticket pressure
- board discipline requests
- supporter complaints
- staffing choices
- stadium maintenance
- derby security bills
- facility upgrades
- community outreach
- emergency maintenance issues

Events are persisted in SQLite (`chairman_events`) and include chosen option tracking.

## 2) Club Identity + League Context

The seeded league now contains 10 clubs with distinct attributes:

- name
- strength
- reputation
- supporter expectation
- fanbase indicator

A rivalry table (`club_rivalries`) creates derby context.

Fixtures are seeded for a compact home/away league schedule.

Standings are computed from persisted results and shown as:

- position
- points
- goal difference

This gives match outcomes immediate league meaning.

## 3) Finance Breakdown

Balance movement is now categorized and visible:

- `matchday_income`
- `wages`
- `upkeep`
- `one_off`
- `board_related`

Each finance entry stores:

- date
- category
- amount
- balance-after snapshot
- description

The finance panel answers “why did my balance move?” without deep accounting complexity.

## 4) Board Objectives

Board objectives are persisted and visible at all times.

Current objective types:

- finish in top half
- finish above named rival
- keep balance above floor
- keep wage bill under cap
- avoid poor recent form

Objectives are evaluated daily and update objective status/progress text.

Board confidence shifts as objective states improve or worsen.

## 5) Supporter Reaction System

Supporter mood now reacts to multiple contexts:

- rivalry match outcomes
- form over last 5 games
- table position vs expectation
- selected chairman event choices
- financial stress context

Supporter notes are logged (`supporter_log`) and shown in the UI so changes are understandable.

## What remains intentionally simplified

This is still a deliberately small prototype, not a full management game.

Still excluded on purpose:

- player database
- transfer market
- tactical simulation depth
- staffing trees
- training systems
- media systems
- cups
- networking/audio

The simulation remains readable and inspectable, with compact interacting systems.

## How the systems interact

- Day advance applies wages and upkeep costs.
- Fixture days simulate matches and update results.
- Results feed standings and drive match consequences.
- Board objective checks update confidence pressure.
- Supporter context check updates supporter mood.
- Decision events create direct chairman agency.
- All key effects persist in SQLite and appear in UI panels.

## UI now shows

- current date
- club summary
- league table
- board objectives + progress
- active decision/event panel with choice buttons
- upcoming fixtures
- recent results
- recent finance ledger entries
- supporter reaction notes
- controls:
  - Advance Day
  - Advance 7 Days
  - Refresh

## Linux dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev libsqlite3-dev
```

Optional diagnostics tools:

```bash
sudo apt install -y sqlite3 gdb valgrind mesa-utils
```

## Build instructions

From repository root:

1) Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

2) Build

```bash
cmake --build build -j
```

## Run instructions

```bash
./build/weekend_empire
```

## Expected runtime behaviour

On startup you should see logs for:

- startup
- schema verification/upgrades
- seed verification
- fixture/world generation when DB is empty

During simulation you should see logs for:

- day advancement
- fixture simulation
- finance changes
- board objective checks
- supporter context updates
- event generation
- event choices

On shutdown you should see cleanup logs.

## Debugging / common failures

### 1) Clean rebuild

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### 2) Verify schema tables

```bash
sqlite3 weekend_empire.db ".tables"
```

Expected milestone tables include:

- leagues
- clubs
- game_state
- fixtures
- finance_events
- board_objectives
- chairman_events
- supporter_log
- club_rivalries

### 3) Quick data checks

```bash
sqlite3 weekend_empire.db "SELECT id,current_date,selected_club_id FROM game_state;"
sqlite3 weekend_empire.db "SELECT id,name,balance,board_confidence,supporter_mood,reputation,supporter_expectation FROM clubs ORDER BY id;"
sqlite3 weekend_empire.db "SELECT id,event_date,event_key,resolved,chosen_option FROM chairman_events ORDER BY id DESC LIMIT 10;"
sqlite3 weekend_empire.db "SELECT id,event_date,category,amount,balance_after,description FROM finance_events ORDER BY id DESC LIMIT 20;"
sqlite3 weekend_empire.db "SELECT id,note_date,reason,delta,mood_after FROM supporter_log ORDER BY id DESC LIMIT 20;"
```

### 4) OpenGL diagnostics

```bash
glxinfo | head -n 20
```

### 5) Runtime debugging

```bash
gdb --args ./build/weekend_empire
valgrind ./build/weekend_empire
```

## Resetting the simulation state

To reset to a fresh seeded world:

```bash
rm -f weekend_empire.db
./build/weekend_empire
```

The app recreates schema and seeds the sample world on next launch.

## What this milestone proves

Milestone 6 proves Weekend Empire can support coherent chairman gameplay loops:

- the player receives grounded decisions with meaningful consequences
- league context and rivalries make results matter
- finances are legible and attributed
- board expectations are explicit and evaluated
- supporters react to both football outcomes and chairman choices

This establishes a practical foundation for future milestones without introducing heavyweight architecture.
