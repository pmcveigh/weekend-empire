# Weekend Empire - Milestone 7 Season Loop, News Feed, and Context Pipeline

Weekend Empire is a Linux-first C++20 football chairman simulator prototype built with SDL2, OpenGL, Dear ImGui, and SQLite.

Milestone 7 restructures the simulation into a clear season lifecycle with a practical world feed and a more grounded chairman-event pipeline.

## 1) What changed in this milestone

- Added an explicit season model in `game_state` with:
  - current season year
  - season start date
  - season end date
  - season phase
  - season summary pending flag
- Linked fixtures, board objectives, and chairman events to a specific season.
- Added a `season_summaries` table and end-of-season summary generation.
- Added a persistent `feed_items` table and a visible News Feed panel in ImGui.
- Reworked progression into a clearer daily pipeline:
  - apply daily finance
  - process fixtures
  - evaluate board objectives
  - generate context-aware events
  - detect season end and create summary
- Added clean season rollover flow with an explicit acknowledgement button.

## 2) How season structure now works

Each season is represented as a start-year label, for example `2026/27`.

- Start date: `YYYY-07-01`
- End date: `YYYY+1-05-31`
- Fixture cadence: weekly Saturday fixture days for league rounds

Phase values are intentionally simple:

- `preseason`
- `opening`
- `regular`
- `run-in`
- `summary`

The UI always shows date, season label, and current phase.

## 3) Season start and end handling

### Season start

On first launch (or after reset), season `2026/27` is initialized.

- Fixtures are generated for that season if absent.
- Board objectives are seeded for that season if absent.
- Feed gets season framing messages as progression begins.

### Season end

Season end is detected when both are true:

- current date is at or beyond season end date
- no unplayed fixtures remain for the current season

When triggered:

- season summary row is created in SQLite
- season summary popup becomes pending in UI
- feed records the completed season line

## 4) Season rollover behavior

Rollover is explicit, not automatic drift:

1. Season summary popup appears.
2. Player clicks `Acknowledge and Start Next Season`.
3. Game transitions to next season start date.
4. Next season fixtures are generated.
5. Next season board objectives are seeded.
6. Old unresolved events are closed.
7. Feed records the new season kickoff.

Continuity preserved:

- balance
- board confidence
- supporter mood
- reputation

Reset/regenerated season elements:

- fixtures
- standings view (implicitly from new-season fixtures/results)
- season board objectives
- season framing in feed

## 5) Season summary contents

The summary stores and displays:

- final league position
- points total
- wins / draws / losses
- ending balance
- board confidence
- supporter mood
- board objectives met vs total

Stored in `season_summaries` and shown in an ImGui modal.

## 6) News/feed panel

A persistent text feed is now part of the UI.

Feed captures grounded chairman-facing updates such as:

- fixture-day announcements
- match results
- metric reactions to results/finances/objective drift
- event generation notices
- finance warning messages
- season run-in and season transition messages

Feed entries are persisted in `feed_items` with:

- date
- season
- club id
- category
- content

## 7) Improved event pipeline

The event layer remains lightweight but now uses context triggers.

Potential trigger sources:

- low balance
- poor recent form
- low supporter mood
- board confidence pressure
- table position relative to expectation
- scheduled weekday windows when no pressure trigger exists

Pipeline per day:

1. Simulation step (advance date)
2. Apply state changes (finance + fixtures)
3. Evaluate derived consequences (board objective drift)
4. Write feed reactions
5. Generate actionable event when triggers match
6. Refresh UI data

Not every feed item is actionable. Actionable chairman decisions and informational world updates are separate but connected.

## 8) What is still intentionally simplified

Still intentionally excluded:

- player-level management
- training plans
- tactical control
- transfer market detail
- dressing room systems
- media simulation depth
- networking/audio

This remains a chairman-level executive simulator prototype.

## 9) UI now shows

- current date
- current season label
- season start/end dates
- season phase
- club summary metrics
- standings table
- upcoming fixtures
- recent results
- current board objectives
- active actionable event with choices
- recent finance entries
- persistent news/feed panel
- season summary popup when season concludes

Controls:

- `Advance Day`
- `Advance 7 Days`
- `Refresh`
- actionable event response buttons
- season summary acknowledgement button

## 10) Build instructions (Linux)

From repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

## 11) Run instructions

```bash
./build/weekend_empire
```

## 12) Required Linux dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev libgl1-mesa-dev libsqlite3-dev
```

Optional diagnostics:

```bash
sudo apt install -y sqlite3 gdb valgrind mesa-utils
```

## 13) Expected runtime behavior

Startup logs include:

- SDL/OpenGL startup details
- DB initialization
- schema verification/upgrades
- season fixture generation when required
- seasonal objective seeding when required

Runtime logs include:

- daily advancement
- fixture processing
- feed item creation
- board objective updates
- event generation and resolution
- season end detection
- season summary creation
- season transition

Shutdown logs include cleanup.

## 14) Debugging steps and common failures

### Clean rebuild

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### Verify schema tables

```bash
sqlite3 weekend_empire.db ".tables"
```

Look for:

- `game_state`
- `fixtures`
- `board_objectives`
- `chairman_events`
- `finance_events`
- `feed_items`
- `season_summaries`

### Inspect season state

```bash
sqlite3 weekend_empire.db "SELECT id,current_date,current_season,season_start_date,season_end_date,season_phase,season_summary_pending FROM game_state;"
sqlite3 weekend_empire.db "SELECT season_year,COUNT(*) FROM fixtures GROUP BY season_year ORDER BY season_year;"
sqlite3 weekend_empire.db "SELECT season_year,summary_date,final_position,points,objectives_met,objectives_total FROM season_summaries ORDER BY season_year DESC;"
sqlite3 weekend_empire.db "SELECT id,item_date,season_year,category,content FROM feed_items ORDER BY id DESC LIMIT 20;"
```

### OpenGL diagnostics

```bash
glxinfo | head -n 20
```

### Runtime debugging

```bash
gdb --args ./build/weekend_empire
valgrind ./build/weekend_empire
```

## 15) Resetting the database / starting fresh

To reset to a clean sample world:

```bash
rm -f weekend_empire.db
./build/weekend_empire
```

On next launch, schema and initial season seed data are recreated automatically.

## 16) What this milestone proves for Weekend Empire

Milestone 7 proves the foundation can support coherent multi-season chairman play without heavy architecture:

- explicit season lifecycle
- deterministic, readable fixture cadence
- practical season close + rollover
- persistent reactive feed
- context-sensitive but compact event generation

This gives a stronger simulation backbone while keeping scope firmly in chairman reality.
