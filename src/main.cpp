#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <sqlite3.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct ClubSummary {
    int id = 0;
    std::string name;
    std::string league_name;
    int balance = 0;
    int running_cost = 0;
    int board_confidence = 50;
    int supporter_mood = 50;
    int strength = 50;
};

struct GameState {
    std::string current_date;
    int selected_club_id = 1;
};

struct FixtureRow {
    int id = 0;
    std::string fixture_date;
    std::string home_name;
    std::string away_name;
    int home_goals = -1;
    int away_goals = -1;
    int played = 0;
};

struct FinanceEvent {
    std::string event_date;
    std::string event_type;
    int amount = 0;
    int balance_after = 0;
    std::string description;
};

bool configure_sdl_gl_attributes() {
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) != 0) {
        std::cerr << "[error] Failed to set OpenGL major version: " << SDL_GetError() << std::endl;
        return false;
    }
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) != 0) {
        std::cerr << "[error] Failed to set OpenGL minor version: " << SDL_GetError() << std::endl;
        return false;
    }
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0) {
        std::cerr << "[error] Failed to set OpenGL profile: " << SDL_GetError() << std::endl;
        return false;
    }
    if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
        std::cerr << "[error] Failed to enable OpenGL double buffering: " << SDL_GetError() << std::endl;
        return false;
    }
    std::cout << "[startup] SDL OpenGL attributes configured (core 3.3, double buffer)" << std::endl;
    return true;
}

void log_gl_string(const char* label, GLenum name) {
    const GLubyte* value = glGetString(name);
    if (value) {
        std::cout << "[startup] " << label << ": " << value << std::endl;
    } else {
        std::cout << "[warn] " << label << " unavailable" << std::endl;
    }
}

bool run_sql(sqlite3* database, const char* sql, const char* stage_label) {
    char* error_message = nullptr;
    if (sqlite3_exec(database, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::cerr << "[error] " << stage_label << " failed: " << (error_message ? error_message : "unknown") << std::endl;
        sqlite3_free(error_message);
        return false;
    }
    std::cout << "[startup] " << stage_label << " complete" << std::endl;
    return true;
}

bool open_database(sqlite3** database, const char* path) {
    std::cout << "[startup] Initialising SQLite" << std::endl;
    if (sqlite3_open(path, database) != SQLITE_OK) {
        std::cerr << "[error] sqlite3_open failed: " << sqlite3_errmsg(*database) << std::endl;
        return false;
    }
    std::cout << "[startup] SQLite opened at " << path << std::endl;
    return true;
}

bool column_exists(sqlite3* database, const char* table_name, const char* column_name) {
    std::string pragma = "PRAGMA table_info(" + std::string(table_name) + ");";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, pragma.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    bool exists = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* name_text = sqlite3_column_text(statement, 1);
        if (name_text && std::string(reinterpret_cast<const char*>(name_text)) == column_name) {
            exists = true;
            break;
        }
    }

    sqlite3_finalize(statement);
    return exists;
}

bool ensure_schema(sqlite3* database) {
    const char* create_sql = R"SQL(
CREATE TABLE IF NOT EXISTS leagues (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS clubs (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    league_id INTEGER NOT NULL,
    balance INTEGER NOT NULL,
    wage_bill INTEGER NOT NULL DEFAULT 18000,
    running_cost INTEGER NOT NULL DEFAULT 2500,
    board_confidence INTEGER NOT NULL DEFAULT 50,
    supporter_mood INTEGER NOT NULL DEFAULT 50,
    strength INTEGER NOT NULL DEFAULT 50,
    FOREIGN KEY (league_id) REFERENCES leagues(id)
);

CREATE TABLE IF NOT EXISTS game_state (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    current_date TEXT NOT NULL,
    selected_club_id INTEGER NOT NULL,
    FOREIGN KEY (selected_club_id) REFERENCES clubs(id)
);

CREATE TABLE IF NOT EXISTS fixtures (
    id INTEGER PRIMARY KEY,
    fixture_date TEXT NOT NULL,
    league_id INTEGER NOT NULL,
    home_club_id INTEGER NOT NULL,
    away_club_id INTEGER NOT NULL,
    played INTEGER NOT NULL DEFAULT 0,
    home_goals INTEGER,
    away_goals INTEGER,
    FOREIGN KEY (league_id) REFERENCES leagues(id),
    FOREIGN KEY (home_club_id) REFERENCES clubs(id),
    FOREIGN KEY (away_club_id) REFERENCES clubs(id)
);

CREATE TABLE IF NOT EXISTS finance_events (
    id INTEGER PRIMARY KEY,
    event_date TEXT NOT NULL,
    club_id INTEGER NOT NULL,
    amount INTEGER NOT NULL,
    balance_after INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    description TEXT NOT NULL,
    FOREIGN KEY (club_id) REFERENCES clubs(id)
);
)SQL";

    if (!run_sql(database, create_sql, "Schema verification")) {
        return false;
    }

    if (!column_exists(database, "clubs", "running_cost") && !run_sql(database, "ALTER TABLE clubs ADD COLUMN running_cost INTEGER NOT NULL DEFAULT 2500;", "Schema upgrade running_cost")) {
        return false;
    }
    if (!column_exists(database, "clubs", "board_confidence") && !run_sql(database, "ALTER TABLE clubs ADD COLUMN board_confidence INTEGER NOT NULL DEFAULT 50;", "Schema upgrade board_confidence")) {
        return false;
    }
    if (!column_exists(database, "clubs", "supporter_mood") && !run_sql(database, "ALTER TABLE clubs ADD COLUMN supporter_mood INTEGER NOT NULL DEFAULT 50;", "Schema upgrade supporter_mood")) {
        return false;
    }
    if (!column_exists(database, "clubs", "strength") && !run_sql(database, "ALTER TABLE clubs ADD COLUMN strength INTEGER NOT NULL DEFAULT 50;", "Schema upgrade strength")) {
        return false;
    }

    return true;
}

bool ensure_seed_data(sqlite3* database) {
    const char* seed_sql = R"SQL(
INSERT OR IGNORE INTO leagues (id, name)
VALUES (1, 'Northern Premier Prototype');

INSERT OR IGNORE INTO clubs (id, name, league_id, balance, wage_bill, running_cost, board_confidence, supporter_mood, strength)
VALUES
    (1, 'Bangor FC', 1, 250000, 18000, 2400, 60, 58, 62),
    (2, 'Afon Athletic', 1, 240000, 17500, 2300, 55, 54, 57),
    (3, 'Meadow Borough', 1, 235000, 17000, 2250, 53, 52, 55),
    (4, 'Peninsula Town', 1, 245000, 17800, 2350, 57, 56, 59);

INSERT OR IGNORE INTO game_state (id, current_date, selected_club_id)
VALUES (1, '2026-01-01', 1);
)SQL";

    if (!run_sql(database, seed_sql, "Sample club and game state verification")) {
        return false;
    }

    const char* fixture_count_sql = "SELECT COUNT(*) FROM fixtures;";
    sqlite3_stmt* count_statement = nullptr;
    if (sqlite3_prepare_v2(database, fixture_count_sql, -1, &count_statement, nullptr) != SQLITE_OK) {
        return false;
    }

    int fixture_count = 0;
    if (sqlite3_step(count_statement) == SQLITE_ROW) {
        fixture_count = sqlite3_column_int(count_statement, 0);
    }
    sqlite3_finalize(count_statement);

    if (fixture_count == 0) {
        std::cout << "[startup] Seeding fixtures" << std::endl;
        const char* fixture_seed_sql = R"SQL(
INSERT INTO fixtures (fixture_date, league_id, home_club_id, away_club_id) VALUES
('2026-01-03', 1, 1, 2),
('2026-01-03', 1, 3, 4),
('2026-01-10', 1, 2, 1),
('2026-01-10', 1, 4, 3),
('2026-01-17', 1, 1, 3),
('2026-01-17', 1, 2, 4),
('2026-01-24', 1, 3, 1),
('2026-01-24', 1, 4, 2),
('2026-01-31', 1, 1, 4),
('2026-01-31', 1, 2, 3),
('2026-02-07', 1, 4, 1),
('2026-02-07', 1, 3, 2);
)SQL";
        if (!run_sql(database, fixture_seed_sql, "Fixture seed")) {
            return false;
        }
    }

    return true;
}

bool parse_date(const std::string& date, std::tm* out_tm) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') {
        return false;
    }

    std::tm tm_value = {};
    tm_value.tm_year = std::stoi(date.substr(0, 4)) - 1900;
    tm_value.tm_mon = std::stoi(date.substr(5, 2)) - 1;
    tm_value.tm_mday = std::stoi(date.substr(8, 2));
    tm_value.tm_hour = 12;

    if (std::mktime(&tm_value) == -1) {
        return false;
    }

    *out_tm = tm_value;
    return true;
}

std::string add_days(const std::string& date, int day_delta) {
    std::tm tm_value = {};
    if (!parse_date(date, &tm_value)) {
        return date;
    }
    tm_value.tm_mday += day_delta;
    std::mktime(&tm_value);

    char buffer[11] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm_value);
    return buffer;
}

int clamp_metric(int value) {
    return std::max(0, std::min(100, value));
}

bool load_game_state(sqlite3* database, GameState* game_state) {
    const char* sql = "SELECT current_date, selected_club_id FROM game_state WHERE id = 1;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        std::cerr << "[error] load_game_state prepare failed: " << sqlite3_errmsg(database) << std::endl;
        return false;
    }

    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* date_text = sqlite3_column_text(statement, 0);
        game_state->current_date = date_text ? reinterpret_cast<const char*>(date_text) : "2026-01-01";
        game_state->selected_club_id = sqlite3_column_int(statement, 1);
        ok = true;
        std::cout << "[runtime] Loaded game state date=" << game_state->current_date
                  << " selected_club=" << game_state->selected_club_id << std::endl;
    }

    sqlite3_finalize(statement);
    return ok;
}

bool save_game_date(sqlite3* database, const std::string& new_date) {
    const char* sql = "UPDATE game_state SET current_date = ? WHERE id = 1;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(statement, 1, new_date.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool load_club_summary(sqlite3* database, int club_id, ClubSummary* summary) {
    const char* sql = R"SQL(
SELECT
    c.id,
    c.name,
    l.name,
    c.balance,
    c.running_cost,
    c.board_confidence,
    c.supporter_mood,
    c.strength
FROM clubs c
JOIN leagues l ON l.id = c.league_id
WHERE c.id = ?;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        std::cerr << "[error] load_club_summary prepare failed: " << sqlite3_errmsg(database) << std::endl;
        return false;
    }

    sqlite3_bind_int(statement, 1, club_id);

    bool loaded = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        summary->id = sqlite3_column_int(statement, 0);
        const unsigned char* club_name = sqlite3_column_text(statement, 1);
        const unsigned char* league_name = sqlite3_column_text(statement, 2);
        summary->name = club_name ? reinterpret_cast<const char*>(club_name) : "Unknown";
        summary->league_name = league_name ? reinterpret_cast<const char*>(league_name) : "Unknown";
        summary->balance = sqlite3_column_int(statement, 3);
        summary->running_cost = sqlite3_column_int(statement, 4);
        summary->board_confidence = sqlite3_column_int(statement, 5);
        summary->supporter_mood = sqlite3_column_int(statement, 6);
        summary->strength = sqlite3_column_int(statement, 7);
        loaded = true;
    }

    sqlite3_finalize(statement);
    return loaded;
}

bool record_finance_event(sqlite3* database, const std::string& event_date, int club_id, int amount, int balance_after, const std::string& event_type, const std::string& description) {
    const char* sql = R"SQL(
INSERT INTO finance_events (event_date, club_id, amount, balance_after, event_type, description)
VALUES (?, ?, ?, ?, ?, ?);
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(statement, 1, event_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, club_id);
    sqlite3_bind_int(statement, 3, amount);
    sqlite3_bind_int(statement, 4, balance_after);
    sqlite3_bind_text(statement, 5, event_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, description.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool apply_club_changes(sqlite3* database, int club_id, int balance_delta, int board_delta, int supporter_delta, const std::string& event_date, const std::string& event_type, const std::string& description) {
    ClubSummary summary;
    if (!load_club_summary(database, club_id, &summary)) {
        return false;
    }

    const int next_balance = summary.balance + balance_delta;
    const int next_board = clamp_metric(summary.board_confidence + board_delta);
    const int next_supporters = clamp_metric(summary.supporter_mood + supporter_delta);

    const char* update_sql = "UPDATE clubs SET balance = ?, board_confidence = ?, supporter_mood = ? WHERE id = ?;";
    sqlite3_stmt* update_statement = nullptr;
    if (sqlite3_prepare_v2(database, update_sql, -1, &update_statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(update_statement, 1, next_balance);
    sqlite3_bind_int(update_statement, 2, next_board);
    sqlite3_bind_int(update_statement, 3, next_supporters);
    sqlite3_bind_int(update_statement, 4, club_id);

    const bool updated = sqlite3_step(update_statement) == SQLITE_DONE;
    sqlite3_finalize(update_statement);
    if (!updated) {
        return false;
    }

    if (balance_delta != 0) {
        if (!record_finance_event(database, event_date, club_id, balance_delta, next_balance, event_type, description)) {
            return false;
        }
    }

    std::cout << "[sim] club=" << club_id << " event=" << event_type << " amount=" << balance_delta
              << " board=" << next_board << " supporters=" << next_supporters << std::endl;
    return true;
}

int load_club_strength(sqlite3* database, int club_id) {
    const char* sql = "SELECT strength FROM clubs WHERE id = ?;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return 50;
    }

    sqlite3_bind_int(statement, 1, club_id);
    int strength = 50;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        strength = sqlite3_column_int(statement, 0);
    }

    sqlite3_finalize(statement);
    return strength;
}

int random_goal_count(std::mt19937& rng, int own_strength, int other_strength, bool home) {
    const double home_bonus = home ? 0.35 : 0.0;
    const double strength_gap = static_cast<double>(own_strength - other_strength) / 25.0;
    double lambda = 1.15 + home_bonus + strength_gap;
    lambda = std::max(0.2, std::min(2.8, lambda));
    std::poisson_distribution<int> dist(lambda);
    return std::min(6, dist(rng));
}

bool simulate_fixture(sqlite3* database, int fixture_id, const std::string& fixture_date, int home_club_id, int away_club_id, int selected_club_id) {
    const int home_strength = load_club_strength(database, home_club_id);
    const int away_strength = load_club_strength(database, away_club_id);

    const unsigned int seed = static_cast<unsigned int>(fixture_id * 131 + home_club_id * 17 + away_club_id * 19);
    std::mt19937 rng(seed);

    const int home_goals = random_goal_count(rng, home_strength, away_strength, true);
    const int away_goals = random_goal_count(rng, away_strength, home_strength, false);

    sqlite3_stmt* update_statement = nullptr;
    const char* update_sql = "UPDATE fixtures SET played = 1, home_goals = ?, away_goals = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(database, update_sql, -1, &update_statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(update_statement, 1, home_goals);
    sqlite3_bind_int(update_statement, 2, away_goals);
    sqlite3_bind_int(update_statement, 3, fixture_id);
    const bool fixture_updated = sqlite3_step(update_statement) == SQLITE_DONE;
    sqlite3_finalize(update_statement);

    if (!fixture_updated) {
        return false;
    }

    std::cout << "[sim] fixture " << fixture_id << " on " << fixture_date << " result " << home_goals << "-" << away_goals << std::endl;

    const int winner = home_goals == away_goals ? 0 : (home_goals > away_goals ? home_club_id : away_club_id);

    if (home_club_id == selected_club_id || away_club_id == selected_club_id) {
        const bool selected_home = home_club_id == selected_club_id;
        int board_delta = 0;
        int supporter_delta = 0;
        int cash_delta = 14000;

        if (winner == 0) {
            board_delta = 1;
            supporter_delta = 1;
            cash_delta += 1000;
        } else if (winner == selected_club_id) {
            board_delta = 4;
            supporter_delta = selected_home ? 6 : 4;
            cash_delta += 5000;
        } else {
            board_delta = -4;
            supporter_delta = selected_home ? -6 : -4;
            cash_delta -= 2000;
        }

        if (selected_home) {
            cash_delta += 6000;
        }

        std::ostringstream description;
        description << "Matchday " << fixture_id << " " << home_goals << "-" << away_goals;
        if (!apply_club_changes(database, selected_club_id, cash_delta, board_delta, supporter_delta, fixture_date, "matchday", description.str())) {
            return false;
        }
    }

    return true;
}

bool process_fixtures_for_date(sqlite3* database, const std::string& date, int selected_club_id) {
    const char* sql = R"SQL(
SELECT id, fixture_date, home_club_id, away_club_id
FROM fixtures
WHERE fixture_date = ? AND played = 0
ORDER BY id;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(statement, 1, date.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = true;
    int processed = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int fixture_id = sqlite3_column_int(statement, 0);
        const unsigned char* fixture_date_text = sqlite3_column_text(statement, 1);
        const int home_club_id = sqlite3_column_int(statement, 2);
        const int away_club_id = sqlite3_column_int(statement, 3);
        std::string fixture_date = fixture_date_text ? reinterpret_cast<const char*>(fixture_date_text) : date;

        if (!simulate_fixture(database, fixture_id, fixture_date, home_club_id, away_club_id, selected_club_id)) {
            ok = false;
            break;
        }
        processed += 1;
    }

    sqlite3_finalize(statement);
    std::cout << "[sim] fixture processing date=" << date << " simulated=" << processed << std::endl;
    return ok;
}

bool apply_daily_cost(sqlite3* database, int selected_club_id, const std::string& date) {
    ClubSummary summary;
    if (!load_club_summary(database, selected_club_id, &summary)) {
        return false;
    }

    const int daily_cost = summary.running_cost;
    int board_delta = 0;
    int supporter_delta = 0;

    if (summary.balance - daily_cost < 0) {
        board_delta = -2;
        supporter_delta = -1;
    } else if (summary.balance > 150000) {
        board_delta = 1;
    }

    std::ostringstream description;
    description << "Daily running cost";
    return apply_club_changes(database, selected_club_id, -daily_cost, board_delta, supporter_delta, date, "running_cost", description.str());
}

bool advance_single_day(sqlite3* database, GameState* game_state) {
    const std::string next_date = add_days(game_state->current_date, 1);
    std::cout << "[sim] advancing day " << game_state->current_date << " -> " << next_date << std::endl;

    if (!run_sql(database, "BEGIN TRANSACTION;", "Begin day transaction")) {
        return false;
    }

    bool ok = apply_daily_cost(database, game_state->selected_club_id, next_date)
        && process_fixtures_for_date(database, next_date, game_state->selected_club_id)
        && save_game_date(database, next_date);

    if (ok) {
        if (!run_sql(database, "COMMIT;", "Commit day transaction")) {
            ok = false;
        }
    }

    if (!ok) {
        run_sql(database, "ROLLBACK;", "Rollback day transaction");
        return false;
    }

    game_state->current_date = next_date;
    return true;
}

bool load_upcoming_fixtures(sqlite3* database, const std::string& current_date, std::vector<FixtureRow>* rows, int limit) {
    const char* sql = R"SQL(
SELECT f.id, f.fixture_date, home.name, away.name, f.played, f.home_goals, f.away_goals
FROM fixtures f
JOIN clubs home ON home.id = f.home_club_id
JOIN clubs away ON away.id = f.away_club_id
WHERE f.fixture_date >= ?
ORDER BY f.fixture_date, f.id
LIMIT ?;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(statement, 1, current_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, limit);

    rows->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        FixtureRow row;
        row.id = sqlite3_column_int(statement, 0);
        const unsigned char* date_text = sqlite3_column_text(statement, 1);
        const unsigned char* home_text = sqlite3_column_text(statement, 2);
        const unsigned char* away_text = sqlite3_column_text(statement, 3);
        row.fixture_date = date_text ? reinterpret_cast<const char*>(date_text) : "?";
        row.home_name = home_text ? reinterpret_cast<const char*>(home_text) : "?";
        row.away_name = away_text ? reinterpret_cast<const char*>(away_text) : "?";
        row.played = sqlite3_column_int(statement, 4);
        row.home_goals = sqlite3_column_type(statement, 5) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 5);
        row.away_goals = sqlite3_column_type(statement, 6) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 6);
        rows->push_back(row);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_recent_results(sqlite3* database, std::vector<FixtureRow>* rows, int limit) {
    const char* sql = R"SQL(
SELECT f.id, f.fixture_date, home.name, away.name, f.played, f.home_goals, f.away_goals
FROM fixtures f
JOIN clubs home ON home.id = f.home_club_id
JOIN clubs away ON away.id = f.away_club_id
WHERE f.played = 1
ORDER BY f.fixture_date DESC, f.id DESC
LIMIT ?;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(statement, 1, limit);

    rows->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        FixtureRow row;
        row.id = sqlite3_column_int(statement, 0);
        const unsigned char* date_text = sqlite3_column_text(statement, 1);
        const unsigned char* home_text = sqlite3_column_text(statement, 2);
        const unsigned char* away_text = sqlite3_column_text(statement, 3);
        row.fixture_date = date_text ? reinterpret_cast<const char*>(date_text) : "?";
        row.home_name = home_text ? reinterpret_cast<const char*>(home_text) : "?";
        row.away_name = away_text ? reinterpret_cast<const char*>(away_text) : "?";
        row.played = sqlite3_column_int(statement, 4);
        row.home_goals = sqlite3_column_int(statement, 5);
        row.away_goals = sqlite3_column_int(statement, 6);
        rows->push_back(row);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_recent_finance_events(sqlite3* database, int club_id, std::vector<FinanceEvent>* events, int limit) {
    const char* sql = R"SQL(
SELECT event_date, event_type, amount, balance_after, description
FROM finance_events
WHERE club_id = ?
ORDER BY id DESC
LIMIT ?;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(statement, 1, club_id);
    sqlite3_bind_int(statement, 2, limit);

    events->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        FinanceEvent event;
        const unsigned char* date_text = sqlite3_column_text(statement, 0);
        const unsigned char* type_text = sqlite3_column_text(statement, 1);
        const unsigned char* description_text = sqlite3_column_text(statement, 4);
        event.event_date = date_text ? reinterpret_cast<const char*>(date_text) : "?";
        event.event_type = type_text ? reinterpret_cast<const char*>(type_text) : "unknown";
        event.amount = sqlite3_column_int(statement, 2);
        event.balance_after = sqlite3_column_int(statement, 3);
        event.description = description_text ? reinterpret_cast<const char*>(description_text) : "";
        events->push_back(event);
    }

    sqlite3_finalize(statement);
    return true;
}

bool refresh_ui_data(sqlite3* database, const GameState& game_state, ClubSummary* club_summary, std::vector<FixtureRow>* upcoming, std::vector<FixtureRow>* recent_results, std::vector<FinanceEvent>* finance_events) {
    return load_club_summary(database, game_state.selected_club_id, club_summary)
        && load_upcoming_fixtures(database, game_state.current_date, upcoming, 8)
        && load_recent_results(database, recent_results, 8)
        && load_recent_finance_events(database, game_state.selected_club_id, finance_events, 10);
}

void render_fixture_row(const FixtureRow& row) {
    if (row.played == 1 && row.home_goals >= 0 && row.away_goals >= 0) {
        ImGui::Text("%s  %s %d - %d %s", row.fixture_date.c_str(), row.home_name.c_str(), row.home_goals, row.away_goals, row.away_name.c_str());
    } else {
        ImGui::Text("%s  %s vs %s", row.fixture_date.c_str(), row.home_name.c_str(), row.away_name.c_str());
    }
}

}

int main(int, char**) {
    std::cout << "[startup] Weekend Empire starting" << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[error] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (!configure_sdl_gl_attributes()) {
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Weekend Empire", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "[error] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context || SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "[error] SDL OpenGL context setup failed: " << SDL_GetError() << std::endl;
        if (gl_context) {
            SDL_GL_DeleteContext(gl_context);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (SDL_GL_SetSwapInterval(1) == 0) {
        std::cout << "[startup] VSync enabled" << std::endl;
    }

    log_gl_string("OpenGL vendor", GL_VENDOR);
    log_gl_string("OpenGL renderer", GL_RENDERER);
    log_gl_string("OpenGL version", GL_VERSION);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context) || !ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "[error] Dear ImGui backend initialisation failed" << std::endl;
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    sqlite3* database = nullptr;
    if (!open_database(&database, "weekend_empire.db") || !ensure_schema(database) || !ensure_seed_data(database)) {
        if (database) {
            sqlite3_close(database);
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GameState game_state;
    if (!load_game_state(database, &game_state)) {
        game_state.current_date = "2026-01-01";
        game_state.selected_club_id = 1;
    }

    ClubSummary club_summary;
    std::vector<FixtureRow> upcoming_fixtures;
    std::vector<FixtureRow> recent_results;
    std::vector<FinanceEvent> finance_events;

    refresh_ui_data(database, game_state, &club_summary, &upcoming_fixtures, &recent_results, &finance_events);

    std::string last_action = "None";
    bool running = true;

    std::cout << "[runtime] Entering main loop" << std::endl;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                std::cout << "[event] Window close requested" << std::endl;
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) {
                std::cout << "[event] Escape pressed, quitting" << std::endl;
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(900.0f, 650.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Weekend Empire Simulation");
        ImGui::Text("Current Date: %s", game_state.current_date.c_str());
        ImGui::Separator();

        ImGui::Text("Club Summary");
        ImGui::Text("Club: %s", club_summary.name.c_str());
        ImGui::Text("League: %s", club_summary.league_name.c_str());
        ImGui::Text("Balance: \xC2\xA3%d", club_summary.balance);
        ImGui::Text("Daily Running Cost: \xC2\xA3%d", club_summary.running_cost);
        ImGui::Text("Board Confidence: %d/100", club_summary.board_confidence);
        ImGui::Text("Supporter Mood: %d/100", club_summary.supporter_mood);

        ImGui::Separator();
        if (ImGui::Button("Advance Day")) {
            std::cout << "[ui] Advance Day pressed" << std::endl;
            if (advance_single_day(database, &game_state)) {
                refresh_ui_data(database, game_state, &club_summary, &upcoming_fixtures, &recent_results, &finance_events);
                last_action = "Advanced 1 day";
            } else {
                last_action = "Advance Day failed";
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Advance 7 Days")) {
            std::cout << "[ui] Advance 7 Days pressed" << std::endl;
            bool ok = true;
            for (int i = 0; i < 7; ++i) {
                if (!advance_single_day(database, &game_state)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                refresh_ui_data(database, game_state, &club_summary, &upcoming_fixtures, &recent_results, &finance_events);
                last_action = "Advanced 7 days";
            } else {
                last_action = "Advance 7 Days failed";
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Refresh Data")) {
            std::cout << "[ui] Refresh Data pressed" << std::endl;
            if (refresh_ui_data(database, game_state, &club_summary, &upcoming_fixtures, &recent_results, &finance_events)) {
                last_action = "Data refreshed";
            } else {
                last_action = "Refresh failed";
            }
        }

        ImGui::Text("Last Action: %s", last_action.c_str());
        ImGui::Separator();

        ImGui::Text("Upcoming Fixtures");
        for (const FixtureRow& row : upcoming_fixtures) {
            if (row.played == 0) {
                render_fixture_row(row);
            }
        }

        ImGui::Separator();
        ImGui::Text("Recent Results");
        for (const FixtureRow& row : recent_results) {
            render_fixture_row(row);
        }

        ImGui::Separator();
        ImGui::Text("Recent Finance Events");
        for (const FinanceEvent& event : finance_events) {
            ImGui::Text("%s  %s  %+d  balance \xC2\xA3%d", event.event_date.c_str(), event.event_type.c_str(), event.amount, event.balance_after);
            ImGui::Text("  %s", event.description.c_str());
        }

        ImGui::End();

        ImGui::Render();
        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
        glViewport(0, 0, drawable_width, drawable_height);
        glClearColor(0.10f, 0.18f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    std::cout << "[shutdown] Cleaning up" << std::endl;
    sqlite3_close(database);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[shutdown] Exit complete" << std::endl;
    return 0;
}
