#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <sqlite3.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct GameState {
    std::string current_date;
    int selected_club_id = 1;
};

struct ClubSummary {
    int id = 0;
    std::string name;
    std::string league_name;
    int balance = 0;
    int wage_bill = 0;
    int running_cost = 0;
    int board_confidence = 50;
    int supporter_mood = 50;
    int strength = 50;
    int reputation = 50;
    int supporter_expectation = 6;
    int fanbase = 50;
};

struct FixtureRow {
    int id = 0;
    std::string fixture_date;
    int home_club_id = 0;
    int away_club_id = 0;
    std::string home_name;
    std::string away_name;
    int home_goals = -1;
    int away_goals = -1;
    int played = 0;
};

struct StandingRow {
    int club_id = 0;
    std::string club_name;
    int played = 0;
    int won = 0;
    int drawn = 0;
    int lost = 0;
    int goals_for = 0;
    int goals_against = 0;
    int goal_diff = 0;
    int points = 0;
};

struct FinanceEvent {
    std::string event_date;
    std::string category;
    int amount = 0;
    int balance_after = 0;
    std::string description;
};

struct BoardObjective {
    int id = 0;
    std::string objective_type;
    std::string title;
    int param_a = 0;
    int param_b = 0;
    int last_score = 0;
    std::string progress_text;
};

struct ChairmanEvent {
    int id = 0;
    std::string event_date;
    std::string event_key;
    std::string title;
    std::string description;
    std::array<std::string, 3> option_label;
    std::array<int, 3> option_balance_delta{};
    std::array<int, 3> option_board_delta{};
    std::array<int, 3> option_supporter_delta{};
    std::array<int, 3> option_reputation_delta{};
    int option_count = 0;
    int resolved = 0;
};

struct SupporterNote {
    std::string note_date;
    std::string reason;
    int delta = 0;
    int mood_after = 0;
};

struct EventTemplate {
    std::string key;
    std::string title;
    std::string description;
    std::array<std::string, 3> labels;
    std::array<int, 3> balance{};
    std::array<int, 3> board{};
    std::array<int, 3> supporters{};
    std::array<int, 3> reputation{};
    int option_count = 2;
};

int clamp_metric(int value) {
    return std::max(0, std::min(100, value));
}

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
    std::cout << "[startup] SDL OpenGL attributes configured" << std::endl;
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

bool run_sql(sqlite3* database, const char* sql, const char* label) {
    char* error_message = nullptr;
    if (sqlite3_exec(database, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
        std::cerr << "[error] " << label << " failed: " << (error_message ? error_message : "unknown") << std::endl;
        sqlite3_free(error_message);
        return false;
    }
    std::cout << "[db] " << label << " complete" << std::endl;
    return true;
}

bool column_exists(sqlite3* database, const char* table_name, const char* column_name) {
    std::string sql = "PRAGMA table_info(" + std::string(table_name) + ");";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool found = false;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(statement, 1);
        if (text && std::string(reinterpret_cast<const char*>(text)) == column_name) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    return found;
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

int weekday_index(const std::string& date) {
    std::tm tm_value = {};
    if (!parse_date(date, &tm_value)) {
        return -1;
    }
    std::mktime(&tm_value);
    return tm_value.tm_wday;
}

bool open_database(sqlite3** database, const char* path) {
    std::cout << "[startup] Initialising SQLite" << std::endl;
    if (sqlite3_open(path, database) != SQLITE_OK) {
        std::cerr << "[error] sqlite3_open failed: " << sqlite3_errmsg(*database) << std::endl;
        return false;
    }
    return true;
}

bool ensure_schema(sqlite3* database) {
    const char* sql = R"SQL(
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
    reputation INTEGER NOT NULL DEFAULT 50,
    supporter_expectation INTEGER NOT NULL DEFAULT 6,
    fanbase INTEGER NOT NULL DEFAULT 50,
    FOREIGN KEY (league_id) REFERENCES leagues(id)
);

CREATE TABLE IF NOT EXISTS game_state (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    current_date TEXT NOT NULL,
    selected_club_id INTEGER NOT NULL,
    FOREIGN KEY (selected_club_id) REFERENCES clubs(id)
);

CREATE TABLE IF NOT EXISTS club_rivalries (
    id INTEGER PRIMARY KEY,
    club_id INTEGER NOT NULL,
    rival_club_id INTEGER NOT NULL,
    UNIQUE(club_id, rival_club_id)
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
    FOREIGN KEY (league_id) REFERENCES leagues(id)
);

CREATE TABLE IF NOT EXISTS finance_events (
    id INTEGER PRIMARY KEY,
    event_date TEXT NOT NULL,
    club_id INTEGER NOT NULL,
    category TEXT NOT NULL,
    amount INTEGER NOT NULL,
    balance_after INTEGER NOT NULL,
    description TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS board_objectives (
    id INTEGER PRIMARY KEY,
    club_id INTEGER NOT NULL,
    objective_type TEXT NOT NULL,
    title TEXT NOT NULL,
    param_a INTEGER NOT NULL DEFAULT 0,
    param_b INTEGER NOT NULL DEFAULT 0,
    last_score INTEGER NOT NULL DEFAULT 0,
    progress_text TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS chairman_events (
    id INTEGER PRIMARY KEY,
    club_id INTEGER NOT NULL,
    event_date TEXT NOT NULL,
    event_key TEXT NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL,
    option_a_label TEXT NOT NULL,
    option_a_balance INTEGER NOT NULL,
    option_a_board INTEGER NOT NULL,
    option_a_supporters INTEGER NOT NULL,
    option_a_reputation INTEGER NOT NULL,
    option_b_label TEXT NOT NULL,
    option_b_balance INTEGER NOT NULL,
    option_b_board INTEGER NOT NULL,
    option_b_supporters INTEGER NOT NULL,
    option_b_reputation INTEGER NOT NULL,
    option_c_label TEXT,
    option_c_balance INTEGER,
    option_c_board INTEGER,
    option_c_supporters INTEGER,
    option_c_reputation INTEGER,
    option_count INTEGER NOT NULL,
    resolved INTEGER NOT NULL DEFAULT 0,
    chosen_option INTEGER,
    resolved_date TEXT
);

CREATE TABLE IF NOT EXISTS supporter_log (
    id INTEGER PRIMARY KEY,
    note_date TEXT NOT NULL,
    club_id INTEGER NOT NULL,
    reason TEXT NOT NULL,
    delta INTEGER NOT NULL,
    mood_after INTEGER NOT NULL
);
)SQL";

    if (!run_sql(database, sql, "schema verification")) {
        return false;
    }

    const std::vector<std::pair<std::string, std::string>> upgrades = {
        {"running_cost", "ALTER TABLE clubs ADD COLUMN running_cost INTEGER NOT NULL DEFAULT 2500;"},
        {"board_confidence", "ALTER TABLE clubs ADD COLUMN board_confidence INTEGER NOT NULL DEFAULT 50;"},
        {"supporter_mood", "ALTER TABLE clubs ADD COLUMN supporter_mood INTEGER NOT NULL DEFAULT 50;"},
        {"strength", "ALTER TABLE clubs ADD COLUMN strength INTEGER NOT NULL DEFAULT 50;"},
        {"reputation", "ALTER TABLE clubs ADD COLUMN reputation INTEGER NOT NULL DEFAULT 50;"},
        {"supporter_expectation", "ALTER TABLE clubs ADD COLUMN supporter_expectation INTEGER NOT NULL DEFAULT 6;"},
        {"fanbase", "ALTER TABLE clubs ADD COLUMN fanbase INTEGER NOT NULL DEFAULT 50;"}
    };

    for (const auto& upgrade : upgrades) {
        if (!column_exists(database, "clubs", upgrade.first.c_str())) {
            if (!run_sql(database, upgrade.second.c_str(), ("schema upgrade " + upgrade.first).c_str())) {
                return false;
            }
        }
    }

    if (!column_exists(database, "finance_events", "category")) {
        if (!run_sql(database, "ALTER TABLE finance_events ADD COLUMN category TEXT NOT NULL DEFAULT 'uncategorised';", "schema upgrade finance category")) {
            return false;
        }
    }

    return true;
}

bool ensure_seed_data(sqlite3* database) {
    const char* seed_sql = R"SQL(
INSERT OR IGNORE INTO leagues (id, name) VALUES (1, 'Northern Counties League');

INSERT OR IGNORE INTO clubs (id, name, league_id, balance, wage_bill, running_cost, board_confidence, supporter_mood, strength, reputation, supporter_expectation, fanbase)
VALUES
(1, 'Bangor FC', 1, 270000, 19000, 2600, 62, 58, 66, 63, 4, 72),
(2, 'Afon Athletic', 1, 242000, 17500, 2400, 57, 54, 58, 56, 7, 55),
(3, 'Meadow Borough', 1, 236000, 16900, 2320, 55, 52, 55, 54, 8, 51),
(4, 'Peninsula Town', 1, 252000, 18100, 2430, 58, 56, 60, 58, 6, 59),
(5, 'Steelworks United', 1, 265000, 19200, 2550, 60, 57, 64, 61, 5, 66),
(6, 'Harbour Celtic', 1, 221000, 16000, 2200, 52, 50, 53, 51, 9, 48),
(7, 'Eastgate Rovers', 1, 214000, 15400, 2160, 51, 49, 51, 50, 10, 44),
(8, 'Riverside Albion', 1, 233000, 17100, 2280, 54, 53, 56, 53, 8, 52),
(9, 'Miners Bridge', 1, 247000, 17800, 2380, 56, 55, 59, 57, 7, 57),
(10, 'Vale County', 1, 228000, 16500, 2240, 53, 51, 54, 52, 9, 49);

INSERT OR IGNORE INTO game_state (id, current_date, selected_club_id) VALUES (1, '2026-07-01', 1);

INSERT OR IGNORE INTO club_rivalries (club_id, rival_club_id) VALUES
(1,5),(5,1),(1,2),(2,1),(3,4),(4,3),(6,10),(10,6),(7,8),(8,7),(2,9),(9,2);
)SQL";

    if (!run_sql(database, seed_sql, "seed data verification")) {
        return false;
    }

    sqlite3_stmt* count_statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM fixtures;", -1, &count_statement, nullptr) != SQLITE_OK) {
        return false;
    }
    int fixture_count = 0;
    if (sqlite3_step(count_statement) == SQLITE_ROW) {
        fixture_count = sqlite3_column_int(count_statement, 0);
    }
    sqlite3_finalize(count_statement);

    if (fixture_count == 0) {
        std::cout << "[startup] Seeding league fixtures" << std::endl;
        std::vector<int> clubs;
        sqlite3_stmt* club_stmt = nullptr;
        if (sqlite3_prepare_v2(database, "SELECT id FROM clubs WHERE league_id = 1 ORDER BY id;", -1, &club_stmt, nullptr) != SQLITE_OK) {
            return false;
        }
        while (sqlite3_step(club_stmt) == SQLITE_ROW) {
            clubs.push_back(sqlite3_column_int(club_stmt, 0));
        }
        sqlite3_finalize(club_stmt);

        std::vector<std::pair<int, int>> pairings;
        for (size_t i = 0; i < clubs.size(); ++i) {
            for (size_t j = i + 1; j < clubs.size(); ++j) {
                pairings.push_back({clubs[i], clubs[j]});
                pairings.push_back({clubs[j], clubs[i]});
            }
        }

        sqlite3_stmt* insert_stmt = nullptr;
        const char* insert_sql = "INSERT INTO fixtures (fixture_date, league_id, home_club_id, away_club_id) VALUES (?, 1, ?, ?);";
        if (sqlite3_prepare_v2(database, insert_sql, -1, &insert_stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        std::string date = "2026-07-04";
        int matches_that_day = 0;
        for (const auto& match : pairings) {
            sqlite3_bind_text(insert_stmt, 1, date.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(insert_stmt, 2, match.first);
            sqlite3_bind_int(insert_stmt, 3, match.second);
            if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                sqlite3_finalize(insert_stmt);
                return false;
            }
            sqlite3_reset(insert_stmt);
            matches_that_day += 1;
            if (matches_that_day >= 5) {
                date = add_days(date, 7);
                matches_that_day = 0;
            }
        }
        sqlite3_finalize(insert_stmt);
    }

    sqlite3_stmt* objective_count_stmt = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM board_objectives WHERE club_id = 1;", -1, &objective_count_stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    int objective_count = 0;
    if (sqlite3_step(objective_count_stmt) == SQLITE_ROW) {
        objective_count = sqlite3_column_int(objective_count_stmt, 0);
    }
    sqlite3_finalize(objective_count_stmt);

    if (objective_count == 0) {
        const char* objective_seed = R"SQL(
INSERT INTO board_objectives (club_id, objective_type, title, param_a, param_b)
VALUES
(1, 'top_half', 'Finish in top half', 5, 0),
(1, 'above_rival', 'Finish above Steelworks United', 5, 0),
(1, 'balance_floor', 'Keep club balance above £180000', 180000, 0),
(1, 'wage_cap', 'Keep wage bill under £20000/day period', 20000, 0),
(1, 'form_floor', 'Avoid poor run: minimum 5 points in last 5 games', 5, 0);
)SQL";
        if (!run_sql(database, objective_seed, "seed board objectives")) {
            return false;
        }
    }

    return true;
}

bool load_game_state(sqlite3* database, GameState* game_state) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT current_date, selected_club_id FROM game_state WHERE id = 1;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* date_text = sqlite3_column_text(statement, 0);
        game_state->current_date = date_text ? reinterpret_cast<const char*>(date_text) : "2026-07-01";
        game_state->selected_club_id = sqlite3_column_int(statement, 1);
        ok = true;
    }
    sqlite3_finalize(statement);
    return ok;
}

bool save_game_date(sqlite3* database, const std::string& next_date) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "UPDATE game_state SET current_date = ? WHERE id = 1;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, next_date.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool load_club_summary(sqlite3* database, int club_id, ClubSummary* summary) {
    const char* sql = R"SQL(
SELECT c.id, c.name, l.name, c.balance, c.wage_bill, c.running_cost, c.board_confidence, c.supporter_mood,
       c.strength, c.reputation, c.supporter_expectation, c.fanbase
FROM clubs c
JOIN leagues l ON l.id = c.league_id
WHERE c.id = ?;
)SQL";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);
    bool ok = false;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        summary->id = sqlite3_column_int(statement, 0);
        summary->name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        summary->league_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        summary->balance = sqlite3_column_int(statement, 3);
        summary->wage_bill = sqlite3_column_int(statement, 4);
        summary->running_cost = sqlite3_column_int(statement, 5);
        summary->board_confidence = sqlite3_column_int(statement, 6);
        summary->supporter_mood = sqlite3_column_int(statement, 7);
        summary->strength = sqlite3_column_int(statement, 8);
        summary->reputation = sqlite3_column_int(statement, 9);
        summary->supporter_expectation = sqlite3_column_int(statement, 10);
        summary->fanbase = sqlite3_column_int(statement, 11);
        ok = true;
    }
    sqlite3_finalize(statement);
    return ok;
}

bool save_club_metrics(sqlite3* database, int club_id, int balance, int board_confidence, int supporter_mood, int reputation) {
    const char* sql = "UPDATE clubs SET balance = ?, board_confidence = ?, supporter_mood = ?, reputation = ? WHERE id = ?;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, balance);
    sqlite3_bind_int(statement, 2, clamp_metric(board_confidence));
    sqlite3_bind_int(statement, 3, clamp_metric(supporter_mood));
    sqlite3_bind_int(statement, 4, clamp_metric(reputation));
    sqlite3_bind_int(statement, 5, club_id);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool record_finance_event(sqlite3* database, const std::string& event_date, int club_id, const std::string& category, int amount, int balance_after, const std::string& description) {
    const char* sql = R"SQL(
INSERT INTO finance_events (event_date, club_id, category, amount, balance_after, description)
VALUES (?, ?, ?, ?, ?, ?);
)SQL";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, event_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, club_id);
    sqlite3_bind_text(statement, 3, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, amount);
    sqlite3_bind_int(statement, 5, balance_after);
    sqlite3_bind_text(statement, 6, description.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool record_supporter_note(sqlite3* database, const std::string& date, int club_id, const std::string& reason, int delta, int mood_after) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "INSERT INTO supporter_log (note_date, club_id, reason, delta, mood_after) VALUES (?, ?, ?, ?, ?);", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, club_id);
    sqlite3_bind_text(statement, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, delta);
    sqlite3_bind_int(statement, 5, mood_after);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

bool apply_metrics_change(sqlite3* database, int club_id, const std::string& date, const std::string& finance_category, int balance_delta, int board_delta, int supporter_delta, int reputation_delta, const std::string& description, bool write_supporter_note) {
    ClubSummary current;
    if (!load_club_summary(database, club_id, &current)) {
        return false;
    }
    int next_balance = current.balance + balance_delta;
    int next_board = clamp_metric(current.board_confidence + board_delta);
    int next_supporters = clamp_metric(current.supporter_mood + supporter_delta);
    int next_reputation = clamp_metric(current.reputation + reputation_delta);

    if (!save_club_metrics(database, club_id, next_balance, next_board, next_supporters, next_reputation)) {
        return false;
    }
    if (balance_delta != 0) {
        if (!record_finance_event(database, date, club_id, finance_category, balance_delta, next_balance, description)) {
            return false;
        }
    }
    if (write_supporter_note && supporter_delta != 0) {
        if (!record_supporter_note(database, date, club_id, description, supporter_delta, next_supporters)) {
            return false;
        }
    }

    std::cout << "[sim] " << description << " balance_delta=" << balance_delta
              << " board_delta=" << board_delta << " supporter_delta=" << supporter_delta
              << " reputation_delta=" << reputation_delta << std::endl;
    return true;
}

int load_strength(sqlite3* database, int club_id) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT strength FROM clubs WHERE id = ?;", -1, &statement, nullptr) != SQLITE_OK) {
        return 50;
    }
    sqlite3_bind_int(statement, 1, club_id);
    int value = 50;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        value = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
}

bool are_rivals(sqlite3* database, int club_id, int other_club_id) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT 1 FROM club_rivalries WHERE club_id = ? AND rival_club_id = ? LIMIT 1;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);
    sqlite3_bind_int(statement, 2, other_club_id);
    bool rivalry = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return rivalry;
}

int random_goal_count(std::mt19937& rng, int own_strength, int other_strength, bool home) {
    double lambda = 1.05 + (home ? 0.25 : 0.0) + static_cast<double>(own_strength - other_strength) / 30.0;
    lambda = std::max(0.2, std::min(2.8, lambda));
    std::poisson_distribution<int> dist(lambda);
    return std::min(6, dist(rng));
}

bool recent_points_last_n(sqlite3* database, int club_id, int n, int* out_points) {
    const char* sql = R"SQL(
SELECT home_club_id, away_club_id, home_goals, away_goals
FROM fixtures
WHERE played = 1 AND (home_club_id = ? OR away_club_id = ?)
ORDER BY fixture_date DESC, id DESC
LIMIT ?;
)SQL";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);
    sqlite3_bind_int(statement, 2, club_id);
    sqlite3_bind_int(statement, 3, n);
    int points = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        int home = sqlite3_column_int(statement, 0);
        int away = sqlite3_column_int(statement, 1);
        int hg = sqlite3_column_int(statement, 2);
        int ag = sqlite3_column_int(statement, 3);
        if (hg == ag) {
            points += 1;
        } else {
            int winner = hg > ag ? home : away;
            if (winner == club_id) {
                points += 3;
            }
        }
    }
    sqlite3_finalize(statement);
    *out_points = points;
    return true;
}

bool load_standings(sqlite3* database, int league_id, std::vector<StandingRow>* rows) {
    const char* club_sql = "SELECT id, name FROM clubs WHERE league_id = ? ORDER BY id;";
    sqlite3_stmt* club_stmt = nullptr;
    if (sqlite3_prepare_v2(database, club_sql, -1, &club_stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(club_stmt, 1, league_id);
    std::map<int, StandingRow> table;
    while (sqlite3_step(club_stmt) == SQLITE_ROW) {
        StandingRow row;
        row.club_id = sqlite3_column_int(club_stmt, 0);
        row.club_name = reinterpret_cast<const char*>(sqlite3_column_text(club_stmt, 1));
        table[row.club_id] = row;
    }
    sqlite3_finalize(club_stmt);

    const char* fixture_sql = R"SQL(
SELECT home_club_id, away_club_id, home_goals, away_goals
FROM fixtures WHERE league_id = ? AND played = 1;
)SQL";
    sqlite3_stmt* fixture_stmt = nullptr;
    if (sqlite3_prepare_v2(database, fixture_sql, -1, &fixture_stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(fixture_stmt, 1, league_id);
    while (sqlite3_step(fixture_stmt) == SQLITE_ROW) {
        int home_id = sqlite3_column_int(fixture_stmt, 0);
        int away_id = sqlite3_column_int(fixture_stmt, 1);
        int hg = sqlite3_column_int(fixture_stmt, 2);
        int ag = sqlite3_column_int(fixture_stmt, 3);
        auto& home = table[home_id];
        auto& away = table[away_id];
        home.played += 1;
        away.played += 1;
        home.goals_for += hg;
        home.goals_against += ag;
        away.goals_for += ag;
        away.goals_against += hg;

        if (hg > ag) {
            home.won += 1;
            away.lost += 1;
            home.points += 3;
        } else if (hg < ag) {
            away.won += 1;
            home.lost += 1;
            away.points += 3;
        } else {
            home.drawn += 1;
            away.drawn += 1;
            home.points += 1;
            away.points += 1;
        }
    }
    sqlite3_finalize(fixture_stmt);

    rows->clear();
    for (auto& [_, row] : table) {
        row.goal_diff = row.goals_for - row.goals_against;
        rows->push_back(row);
    }

    std::sort(rows->begin(), rows->end(), [](const StandingRow& a, const StandingRow& b) {
        if (a.points != b.points) return a.points > b.points;
        if (a.goal_diff != b.goal_diff) return a.goal_diff > b.goal_diff;
        if (a.goals_for != b.goals_for) return a.goals_for > b.goals_for;
        return a.club_name < b.club_name;
    });

    return true;
}

int table_position_for(std::vector<StandingRow>& standings, int club_id) {
    for (size_t i = 0; i < standings.size(); ++i) {
        if (standings[i].club_id == club_id) {
            return static_cast<int>(i) + 1;
        }
    }
    return static_cast<int>(standings.size());
}

bool simulate_fixture(sqlite3* database, const FixtureRow& fixture, int selected_club_id) {
    int home_strength = load_strength(database, fixture.home_club_id);
    int away_strength = load_strength(database, fixture.away_club_id);
    std::mt19937 rng(static_cast<unsigned int>(fixture.id * 97 + fixture.home_club_id * 13 + fixture.away_club_id * 29));
    int hg = random_goal_count(rng, home_strength, away_strength, true);
    int ag = random_goal_count(rng, away_strength, home_strength, false);

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "UPDATE fixtures SET played = 1, home_goals = ?, away_goals = ? WHERE id = ?;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, hg);
    sqlite3_bind_int(statement, 2, ag);
    sqlite3_bind_int(statement, 3, fixture.id);
    bool updated = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!updated) {
        return false;
    }

    std::cout << "[sim] fixture " << fixture.id << " " << fixture.home_name << " " << hg << "-" << ag << " " << fixture.away_name << std::endl;

    if (fixture.home_club_id != selected_club_id && fixture.away_club_id != selected_club_id) {
        return true;
    }

    bool selected_home = fixture.home_club_id == selected_club_id;
    int opponent = selected_home ? fixture.away_club_id : fixture.home_club_id;
    bool derby = are_rivals(database, selected_club_id, opponent);

    int balance_delta = 13000 + (selected_home ? 7000 : 2500);
    int board_delta = 0;
    int supporter_delta = 0;
    int reputation_delta = 0;

    if (hg == ag) {
        board_delta += 1;
        supporter_delta += 1;
        balance_delta += 1000;
    } else {
        bool won = (selected_home && hg > ag) || (!selected_home && ag > hg);
        if (won) {
            board_delta += derby ? 5 : 3;
            supporter_delta += derby ? 8 : 5;
            reputation_delta += derby ? 3 : 2;
            balance_delta += 4000;
        } else {
            board_delta -= derby ? 6 : 4;
            supporter_delta -= derby ? 10 : 6;
            reputation_delta -= derby ? 3 : 1;
            balance_delta -= 2500;
        }
    }

    std::ostringstream reason;
    reason << "Matchday " << fixture.home_name << " " << hg << "-" << ag << " " << fixture.away_name;

    if (!apply_metrics_change(database, selected_club_id, fixture.fixture_date, "matchday_income", balance_delta, board_delta, supporter_delta, reputation_delta, reason.str(), true)) {
        return false;
    }

    return true;
}

bool load_fixtures_for_date(sqlite3* database, const std::string& date, std::vector<FixtureRow>* rows) {
    const char* sql = R"SQL(
SELECT f.id, f.fixture_date, f.home_club_id, f.away_club_id, home.name, away.name
FROM fixtures f
JOIN clubs home ON home.id = f.home_club_id
JOIN clubs away ON away.id = f.away_club_id
WHERE f.fixture_date = ? AND f.played = 0
ORDER BY f.id;
)SQL";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(statement, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    rows->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        FixtureRow row;
        row.id = sqlite3_column_int(statement, 0);
        row.fixture_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.home_club_id = sqlite3_column_int(statement, 2);
        row.away_club_id = sqlite3_column_int(statement, 3);
        row.home_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        row.away_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        rows->push_back(row);
    }
    sqlite3_finalize(statement);
    return true;
}

bool process_fixtures_for_date(sqlite3* database, const std::string& date, int selected_club_id) {
    std::vector<FixtureRow> fixtures;
    if (!load_fixtures_for_date(database, date, &fixtures)) {
        return false;
    }
    for (const auto& fixture : fixtures) {
        if (!simulate_fixture(database, fixture, selected_club_id)) {
            return false;
        }
    }
    std::cout << "[sim] fixture processing date=" << date << " count=" << fixtures.size() << std::endl;
    return true;
}

bool apply_daily_finance(sqlite3* database, int club_id, const std::string& date) {
    ClubSummary club;
    if (!load_club_summary(database, club_id, &club)) {
        return false;
    }

    int wage_daily = club.wage_bill;
    int upkeep_daily = club.running_cost;
    int board_delta = club.balance > 220000 ? 1 : 0;
    int supporter_delta = 0;
    if (club.balance < 120000) {
        board_delta -= 2;
        supporter_delta -= 1;
    }

    if (!apply_metrics_change(database, club_id, date, "wages", -wage_daily, board_delta, supporter_delta, 0, "Daily wages", false)) {
        return false;
    }
    if (!apply_metrics_change(database, club_id, date, "upkeep", -upkeep_daily, 0, 0, 0, "Daily upkeep", false)) {
        return false;
    }
    return true;
}

std::vector<EventTemplate> build_event_templates() {
    return {
        {"sponsor_offer", "Regional Sponsor Offer", "A local logistics company offers a sleeve sponsorship.", {"Accept full deal", "Negotiate values-first", "Reject public branding"}, {18000, 10000, 0}, {2, 1, -1}, {-2, 1, 2}, {0, 1, 1}, 3},
        {"ticket_price", "Ticket Price Pressure", "Board asks for a modest ticket increase due to costs.", {"Raise prices", "Hold prices", "Run discount week"}, {12000, 0, -8000}, {2, -1, -2}, {-8, 2, 5}, {0, 0, 1}, 3},
        {"stadium_repairs", "Stadium Lighting Fault", "Floodlight repairs are required before next home game.", {"Full repair now", "Temporary patch", "Delay and risk fine"}, {-15000, -6000, -2000}, {1, -1, -4}, {1, -2, -5}, {1, -1, -2}, 3},
        {"community_day", "Community Outreach Request", "Council asks club to run a school outreach day.", {"Fund full event", "Send limited delegation", "Decline"}, {-7000, -2000, 0}, {1, 0, -2}, {6, 2, -4}, {2, 1, -2}, 3},
        {"security_bill", "Derby Security Invoice", "Police request additional derby-day security funding.", {"Pay in full", "Split cost with league", "Contest invoice"}, {-9000, -5000, -1000}, {2, 0, -3}, {3, 0, -5}, {1, 0, -2}, 3},
        {"staff_cost", "Staffing Budget Choice", "Director of football asks for analyst hire.", {"Approve hire", "Approve short contract", "Freeze hiring"}, {-11000, -4000, 0}, {1, 0, -2}, {2, 0, -3}, {1, 0, -1}, 3},
        {"facility_upgrade", "Training Pitch Upgrade", "Small grant can part-fund better drainage.", {"Pay club share", "Postpone decision", "Decline upgrade"}, {-13000, -2000, 0}, {1, 0, -1}, {3, 0, -2}, {2, 0, -1}, 3},
        {"board_request", "Board Wants Cost Discipline", "Board asks for a visible wage discipline statement.", {"Commit to strict cap", "Balanced statement", "Refuse public pledge"}, {4000, 0, 0}, {3, 1, -3}, {-2, 0, -1}, {-1, 0, 0}, 3},
        {"fan_complaint", "Supporter Group Complaint", "Fans ask for cheaper away travel subsidy.", {"Provide subsidy", "Partial support", "No subsidy"}, {-8000, -3000, 0}, {0, 0, -2}, {5, 2, -4}, {1, 0, -1}, 3},
        {"academy_drive", "Youth Open Trial Week", "Academy coaches propose a town trial event.", {"Back it fully", "Low-key trial", "Skip this year"}, {-6000, -2000, 0}, {1, 0, -1}, {4, 1, -2}, {2, 1, 0}, 3},
        {"emergency_boiler", "Training Ground Boiler Failure", "Boiler fails and risks player prep quality.", {"Emergency replacement", "Temporary rental", "Delay work"}, {-10000, -3500, -500}, {1, 0, -2}, {1, -1, -3}, {0, 0, -1}, 3},
        {"sponsor_activation", "Sponsor Activation Demand", "Main sponsor asks for costly fan-zone activation.", {"Deliver full package", "Scaled package", "Refuse activation"}, {-5000, -2000, 0}, {2, 0, -2}, {3, 1, -2}, {1, 0, -2}, 3}
    };
}

bool has_active_event(sqlite3* database, int club_id) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT 1 FROM chairman_events WHERE club_id = ? AND resolved = 0 LIMIT 1;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);
    bool exists = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return exists;
}

bool create_event_from_template(sqlite3* database, int club_id, const std::string& date, const EventTemplate& tmpl) {
    const char* sql = R"SQL(
INSERT INTO chairman_events (
club_id, event_date, event_key, title, description,
option_a_label, option_a_balance, option_a_board, option_a_supporters, option_a_reputation,
option_b_label, option_b_balance, option_b_board, option_b_supporters, option_b_reputation,
option_c_label, option_c_balance, option_c_board, option_c_supporters, option_c_reputation,
option_count, resolved)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0);
)SQL";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(statement, 1, club_id);
    sqlite3_bind_text(statement, 2, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, tmpl.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, tmpl.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, tmpl.description.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text(statement, 6, tmpl.labels[0].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 7, tmpl.balance[0]);
    sqlite3_bind_int(statement, 8, tmpl.board[0]);
    sqlite3_bind_int(statement, 9, tmpl.supporters[0]);
    sqlite3_bind_int(statement, 10, tmpl.reputation[0]);

    sqlite3_bind_text(statement, 11, tmpl.labels[1].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 12, tmpl.balance[1]);
    sqlite3_bind_int(statement, 13, tmpl.board[1]);
    sqlite3_bind_int(statement, 14, tmpl.supporters[1]);
    sqlite3_bind_int(statement, 15, tmpl.reputation[1]);

    if (tmpl.option_count >= 3) {
        sqlite3_bind_text(statement, 16, tmpl.labels[2].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 17, tmpl.balance[2]);
        sqlite3_bind_int(statement, 18, tmpl.board[2]);
        sqlite3_bind_int(statement, 19, tmpl.supporters[2]);
        sqlite3_bind_int(statement, 20, tmpl.reputation[2]);
    } else {
        sqlite3_bind_null(statement, 16);
        sqlite3_bind_null(statement, 17);
        sqlite3_bind_null(statement, 18);
        sqlite3_bind_null(statement, 19);
        sqlite3_bind_null(statement, 20);
    }
    sqlite3_bind_int(statement, 21, tmpl.option_count);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (ok) {
        std::cout << "[event] generated " << tmpl.key << " for " << date << std::endl;
    }
    return ok;
}

bool maybe_generate_daily_event(sqlite3* database, int club_id, const std::string& date) {
    if (has_active_event(database, club_id)) {
        return true;
    }

    int wday = weekday_index(date);
    if (wday != 1 && wday != 3 && wday != 5) {
        return true;
    }

    std::hash<std::string> hasher;
    size_t value = hasher(date + std::to_string(club_id));
    if (value % 100 > 46) {
        return true;
    }

    auto templates = build_event_templates();
    const EventTemplate& chosen = templates[value % templates.size()];
    return create_event_from_template(database, club_id, date, chosen);
}

bool load_active_event(sqlite3* database, int club_id, std::optional<ChairmanEvent>* event) {
    *event = std::nullopt;
    const char* sql = R"SQL(
SELECT id, event_date, event_key, title, description,
       option_a_label, option_a_balance, option_a_board, option_a_supporters, option_a_reputation,
       option_b_label, option_b_balance, option_b_board, option_b_supporters, option_b_reputation,
       option_c_label, option_c_balance, option_c_board, option_c_supporters, option_c_reputation,
       option_count, resolved
FROM chairman_events
WHERE club_id = ? AND resolved = 0
ORDER BY id ASC
LIMIT 1;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);

    if (sqlite3_step(statement) == SQLITE_ROW) {
        ChairmanEvent row;
        row.id = sqlite3_column_int(statement, 0);
        row.event_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.event_key = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        row.title = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        row.description = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));

        row.option_label[0] = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        row.option_balance_delta[0] = sqlite3_column_int(statement, 6);
        row.option_board_delta[0] = sqlite3_column_int(statement, 7);
        row.option_supporter_delta[0] = sqlite3_column_int(statement, 8);
        row.option_reputation_delta[0] = sqlite3_column_int(statement, 9);

        row.option_label[1] = reinterpret_cast<const char*>(sqlite3_column_text(statement, 10));
        row.option_balance_delta[1] = sqlite3_column_int(statement, 11);
        row.option_board_delta[1] = sqlite3_column_int(statement, 12);
        row.option_supporter_delta[1] = sqlite3_column_int(statement, 13);
        row.option_reputation_delta[1] = sqlite3_column_int(statement, 14);

        if (sqlite3_column_type(statement, 15) != SQLITE_NULL) {
            row.option_label[2] = reinterpret_cast<const char*>(sqlite3_column_text(statement, 15));
        }
        row.option_balance_delta[2] = sqlite3_column_type(statement, 16) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, 16);
        row.option_board_delta[2] = sqlite3_column_type(statement, 17) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, 17);
        row.option_supporter_delta[2] = sqlite3_column_type(statement, 18) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, 18);
        row.option_reputation_delta[2] = sqlite3_column_type(statement, 19) == SQLITE_NULL ? 0 : sqlite3_column_int(statement, 19);

        row.option_count = sqlite3_column_int(statement, 20);
        row.resolved = sqlite3_column_int(statement, 21);

        *event = row;
    }

    sqlite3_finalize(statement);
    return true;
}

bool resolve_event_choice(sqlite3* database, int club_id, const std::string& date, const ChairmanEvent& event, int option_index) {
    if (option_index < 0 || option_index >= event.option_count) {
        return false;
    }

    std::ostringstream description;
    description << "Event: " << event.title << " -> " << event.option_label[option_index];

    if (!apply_metrics_change(database, club_id, date, "one_off", event.option_balance_delta[option_index], event.option_board_delta[option_index], event.option_supporter_delta[option_index], event.option_reputation_delta[option_index], description.str(), true)) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "UPDATE chairman_events SET resolved = 1, chosen_option = ?, resolved_date = ? WHERE id = ?;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, option_index + 1);
    sqlite3_bind_text(statement, 2, date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, event.id);
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    std::cout << "[event] resolved event_id=" << event.id << " option=" << (option_index + 1) << std::endl;
    return ok;
}

bool evaluate_board_objectives(sqlite3* database, int club_id, int league_id, const std::string& date, int* net_delta) {
    std::vector<StandingRow> standings;
    if (!load_standings(database, league_id, &standings)) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT id, objective_type, title, param_a, param_b, last_score FROM board_objectives WHERE club_id = ? ORDER BY id;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);

    int objective_drift = 0;

    while (sqlite3_step(statement) == SQLITE_ROW) {
        int id = sqlite3_column_int(statement, 0);
        std::string type = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        std::string title = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        int param_a = sqlite3_column_int(statement, 3);
        int param_b = sqlite3_column_int(statement, 4);
        int last_score = sqlite3_column_int(statement, 5);

        int score = 0;
        std::string progress;

        if (type == "top_half") {
            int position = table_position_for(standings, club_id);
            score = position <= param_a ? 1 : -1;
            progress = "Position " + std::to_string(position) + " / target " + std::to_string(param_a);
        } else if (type == "above_rival") {
            int my_pos = table_position_for(standings, club_id);
            int rival_pos = table_position_for(standings, param_a);
            score = my_pos < rival_pos ? 1 : -1;
            progress = "Your rank " + std::to_string(my_pos) + ", rival rank " + std::to_string(rival_pos);
        } else if (type == "balance_floor") {
            ClubSummary club;
            if (!load_club_summary(database, club_id, &club)) {
                sqlite3_finalize(statement);
                return false;
            }
            score = club.balance >= param_a ? 1 : -2;
            progress = "Balance £" + std::to_string(club.balance) + " / floor £" + std::to_string(param_a);
        } else if (type == "wage_cap") {
            ClubSummary club;
            if (!load_club_summary(database, club_id, &club)) {
                sqlite3_finalize(statement);
                return false;
            }
            score = club.wage_bill <= param_a ? 1 : -1;
            progress = "Wages £" + std::to_string(club.wage_bill) + " / cap £" + std::to_string(param_a);
        } else if (type == "form_floor") {
            int points = 0;
            if (!recent_points_last_n(database, club_id, 5, &points)) {
                sqlite3_finalize(statement);
                return false;
            }
            score = points >= param_a ? 1 : -2;
            progress = "Last 5 matches points " + std::to_string(points) + " / target " + std::to_string(param_a);
        } else {
            progress = "Unknown objective";
        }

        objective_drift += score - last_score;

        sqlite3_stmt* update_stmt = nullptr;
        if (sqlite3_prepare_v2(database, "UPDATE board_objectives SET last_score = ?, progress_text = ? WHERE id = ?;", -1, &update_stmt, nullptr) != SQLITE_OK) {
            sqlite3_finalize(statement);
            return false;
        }
        sqlite3_bind_int(update_stmt, 1, score);
        sqlite3_bind_text(update_stmt, 2, progress.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(update_stmt, 3, id);
        bool update_ok = sqlite3_step(update_stmt) == SQLITE_DONE;
        sqlite3_finalize(update_stmt);
        if (!update_ok) {
            sqlite3_finalize(statement);
            return false;
        }

        std::cout << "[board] " << title << " score=" << score << " progress=" << progress << std::endl;
    }

    sqlite3_finalize(statement);

    if (objective_drift != 0) {
        if (!apply_metrics_change(database, club_id, date, "board_related", 0, objective_drift, 0, 0, "Board objective review", false)) {
            return false;
        }
    }

    *net_delta = objective_drift;
    return true;
}

bool evaluate_supporter_context(sqlite3* database, int club_id, int league_id, const std::string& date) {
    ClubSummary club;
    if (!load_club_summary(database, club_id, &club)) {
        return false;
    }
    std::vector<StandingRow> standings;
    if (!load_standings(database, league_id, &standings)) {
        return false;
    }

    int form_points = 0;
    if (!recent_points_last_n(database, club_id, 5, &form_points)) {
        return false;
    }

    int position = table_position_for(standings, club_id);
    int supporter_delta = 0;
    std::string reason = "Supporter context";

    if (form_points >= 10) {
        supporter_delta += 2;
        reason = "Strong run of form";
    } else if (form_points <= 3) {
        supporter_delta -= 2;
        reason = "Poor run of form";
    }

    if (position <= club.supporter_expectation) {
        supporter_delta += 1;
    } else if (position >= club.supporter_expectation + 3) {
        supporter_delta -= 2;
        reason = "League position below expectation";
    }

    if (club.balance < 110000) {
        supporter_delta -= 1;
        reason = "Financial worries in supporter base";
    }

    if (supporter_delta != 0) {
        if (!apply_metrics_change(database, club_id, date, "board_related", 0, 0, supporter_delta, 0, reason, true)) {
            return false;
        }
    }
    std::cout << "[supporters] mood context update delta=" << supporter_delta << std::endl;
    return true;
}

bool advance_single_day(sqlite3* database, GameState* game_state) {
    std::string next_date = add_days(game_state->current_date, 1);
    std::cout << "[sim] advance day " << game_state->current_date << " -> " << next_date << std::endl;

    if (!run_sql(database, "BEGIN TRANSACTION;", "begin transaction")) {
        return false;
    }

    int objective_delta = 0;

    bool ok = apply_daily_finance(database, game_state->selected_club_id, next_date)
           && process_fixtures_for_date(database, next_date, game_state->selected_club_id)
           && evaluate_board_objectives(database, game_state->selected_club_id, 1, next_date, &objective_delta)
           && evaluate_supporter_context(database, game_state->selected_club_id, 1, next_date)
           && maybe_generate_daily_event(database, game_state->selected_club_id, next_date)
           && save_game_date(database, next_date);

    if (ok) {
        ok = run_sql(database, "COMMIT;", "commit transaction");
    }
    if (!ok) {
        run_sql(database, "ROLLBACK;", "rollback transaction");
        return false;
    }

    std::cout << "[sim] day complete objective_delta=" << objective_delta << std::endl;
    game_state->current_date = next_date;
    return true;
}

bool load_upcoming_fixtures(sqlite3* database, const std::string& current_date, std::vector<FixtureRow>* rows, int limit) {
    const char* sql = R"SQL(
SELECT f.id, f.fixture_date, f.home_club_id, f.away_club_id, home.name, away.name, f.played, f.home_goals, f.away_goals
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
        row.fixture_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.home_club_id = sqlite3_column_int(statement, 2);
        row.away_club_id = sqlite3_column_int(statement, 3);
        row.home_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        row.away_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        row.played = sqlite3_column_int(statement, 6);
        row.home_goals = sqlite3_column_type(statement, 7) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 7);
        row.away_goals = sqlite3_column_type(statement, 8) == SQLITE_NULL ? -1 : sqlite3_column_int(statement, 8);
        rows->push_back(row);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_recent_results(sqlite3* database, std::vector<FixtureRow>* rows, int limit) {
    const char* sql = R"SQL(
SELECT f.id, f.fixture_date, f.home_club_id, f.away_club_id, home.name, away.name, f.played, f.home_goals, f.away_goals
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
        row.fixture_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.home_club_id = sqlite3_column_int(statement, 2);
        row.away_club_id = sqlite3_column_int(statement, 3);
        row.home_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        row.away_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        row.played = sqlite3_column_int(statement, 6);
        row.home_goals = sqlite3_column_int(statement, 7);
        row.away_goals = sqlite3_column_int(statement, 8);
        rows->push_back(row);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_recent_finance_events(sqlite3* database, int club_id, std::vector<FinanceEvent>* events, int limit) {
    const char* sql = R"SQL(
SELECT event_date, category, amount, balance_after, description
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
        event.event_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        event.category = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        event.amount = sqlite3_column_int(statement, 2);
        event.balance_after = sqlite3_column_int(statement, 3);
        event.description = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        events->push_back(event);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_board_objectives(sqlite3* database, int club_id, std::vector<BoardObjective>* objectives) {
    const char* sql = "SELECT id, objective_type, title, param_a, param_b, last_score, progress_text FROM board_objectives WHERE club_id = ? ORDER BY id;";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);

    objectives->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        BoardObjective row;
        row.id = sqlite3_column_int(statement, 0);
        row.objective_type = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.title = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        row.param_a = sqlite3_column_int(statement, 3);
        row.param_b = sqlite3_column_int(statement, 4);
        row.last_score = sqlite3_column_int(statement, 5);
        row.progress_text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 6));
        objectives->push_back(row);
    }

    sqlite3_finalize(statement);
    return true;
}

bool load_supporter_notes(sqlite3* database, int club_id, std::vector<SupporterNote>* notes, int limit) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT note_date, reason, delta, mood_after FROM supporter_log WHERE club_id = ? ORDER BY id DESC LIMIT ?;", -1, &statement, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(statement, 1, club_id);
    sqlite3_bind_int(statement, 2, limit);
    notes->clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        SupporterNote row;
        row.note_date = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        row.reason = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        row.delta = sqlite3_column_int(statement, 2);
        row.mood_after = sqlite3_column_int(statement, 3);
        notes->push_back(row);
    }
    sqlite3_finalize(statement);
    return true;
}

bool refresh_ui_data(sqlite3* database,
                     const GameState& game_state,
                     ClubSummary* club_summary,
                     std::vector<StandingRow>* standings,
                     std::vector<FixtureRow>* upcoming,
                     std::vector<FixtureRow>* recent_results,
                     std::vector<FinanceEvent>* finance_events,
                     std::vector<BoardObjective>* board_objectives,
                     std::vector<SupporterNote>* supporter_notes,
                     std::optional<ChairmanEvent>* active_event) {
    return load_club_summary(database, game_state.selected_club_id, club_summary)
        && load_standings(database, 1, standings)
        && load_upcoming_fixtures(database, game_state.current_date, upcoming, 10)
        && load_recent_results(database, recent_results, 10)
        && load_recent_finance_events(database, game_state.selected_club_id, finance_events, 16)
        && load_board_objectives(database, game_state.selected_club_id, board_objectives)
        && load_supporter_notes(database, game_state.selected_club_id, supporter_notes, 8)
        && load_active_event(database, game_state.selected_club_id, active_event);
}

void render_fixture_row(const FixtureRow& row) {
    if (row.played == 1) {
        ImGui::Text("%s  %s %d-%d %s", row.fixture_date.c_str(), row.home_name.c_str(), row.home_goals, row.away_goals, row.away_name.c_str());
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

    SDL_Window* window = SDL_CreateWindow("Weekend Empire", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1360, 780, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "[error] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context || SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "[error] SDL OpenGL context setup failed: " << SDL_GetError() << std::endl;
        if (gl_context) SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    log_gl_string("OpenGL vendor", GL_VENDOR);
    log_gl_string("OpenGL renderer", GL_RENDERER);
    log_gl_string("OpenGL version", GL_VERSION);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context) || !ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "[error] Dear ImGui backend setup failed" << std::endl;
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    sqlite3* database = nullptr;
    if (!open_database(&database, "weekend_empire.db") || !ensure_schema(database) || !ensure_seed_data(database)) {
        if (database) sqlite3_close(database);
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
        game_state.current_date = "2026-07-01";
        game_state.selected_club_id = 1;
    }

    ClubSummary club_summary;
    std::vector<StandingRow> standings;
    std::vector<FixtureRow> upcoming_fixtures;
    std::vector<FixtureRow> recent_results;
    std::vector<FinanceEvent> finance_events;
    std::vector<BoardObjective> board_objectives;
    std::vector<SupporterNote> supporter_notes;
    std::optional<ChairmanEvent> active_event;

    refresh_ui_data(database, game_state, &club_summary, &standings, &upcoming_fixtures, &recent_results, &finance_events, &board_objectives, &supporter_notes, &active_event);

    std::string last_action = "Ready";
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(1280, 760), ImGuiCond_FirstUseEver);
        ImGui::Begin("Weekend Empire Chairman Prototype");

        ImGui::Text("Date: %s", game_state.current_date.c_str());
        ImGui::Text("Club: %s (%s)", club_summary.name.c_str(), club_summary.league_name.c_str());
        ImGui::Text("Balance: \xC2\xA3%d  Wages: \xC2\xA3%d/day  Upkeep: \xC2\xA3%d/day", club_summary.balance, club_summary.wage_bill, club_summary.running_cost);
        ImGui::Text("Board Confidence: %d/100  Supporter Mood: %d/100  Reputation: %d/100", club_summary.board_confidence, club_summary.supporter_mood, club_summary.reputation);

        if (ImGui::Button("Advance Day")) {
            if (advance_single_day(database, &game_state)) {
                refresh_ui_data(database, game_state, &club_summary, &standings, &upcoming_fixtures, &recent_results, &finance_events, &board_objectives, &supporter_notes, &active_event);
                last_action = "Advanced day";
            } else {
                last_action = "Advance day failed";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Advance 7 Days")) {
            bool ok = true;
            for (int i = 0; i < 7; ++i) {
                if (!advance_single_day(database, &game_state)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                refresh_ui_data(database, game_state, &club_summary, &standings, &upcoming_fixtures, &recent_results, &finance_events, &board_objectives, &supporter_notes, &active_event);
                last_action = "Advanced 7 days";
            } else {
                last_action = "Advance 7 days failed";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            if (refresh_ui_data(database, game_state, &club_summary, &standings, &upcoming_fixtures, &recent_results, &finance_events, &board_objectives, &supporter_notes, &active_event)) {
                last_action = "Data refreshed";
            } else {
                last_action = "Refresh failed";
            }
        }

        ImGui::Text("Last action: %s", last_action.c_str());
        ImGui::Separator();

        ImGui::Columns(2, "top_cols", true);

        ImGui::Text("League Table");
        ImGui::Separator();
        for (size_t i = 0; i < standings.size(); ++i) {
            const auto& row = standings[i];
            ImGui::Text("%2d. %-18s %2d pts  GD %+d", static_cast<int>(i) + 1, row.club_name.c_str(), row.points, row.goal_diff);
        }

        ImGui::Spacing();
        ImGui::Text("Board Objectives");
        ImGui::Separator();
        for (const auto& obj : board_objectives) {
            const char* status = obj.last_score > 0 ? "On Track" : (obj.last_score < 0 ? "At Risk" : "Neutral");
            ImGui::Text("%s [%s]", obj.title.c_str(), status);
            ImGui::TextWrapped("%s", obj.progress_text.c_str());
        }

        ImGui::NextColumn();

        ImGui::Text("Decision/Event Panel");
        ImGui::Separator();
        if (active_event.has_value()) {
            const ChairmanEvent& ev = *active_event;
            ImGui::Text("%s (%s)", ev.title.c_str(), ev.event_date.c_str());
            ImGui::TextWrapped("%s", ev.description.c_str());
            for (int i = 0; i < ev.option_count; ++i) {
                std::string button = std::to_string(i + 1) + ". " + ev.option_label[i];
                if (ImGui::Button(button.c_str())) {
                    if (resolve_event_choice(database, game_state.selected_club_id, game_state.current_date, ev, i)) {
                        refresh_ui_data(database, game_state, &club_summary, &standings, &upcoming_fixtures, &recent_results, &finance_events, &board_objectives, &supporter_notes, &active_event);
                        last_action = "Event choice applied";
                    } else {
                        last_action = "Event choice failed";
                    }
                }
                ImGui::Text("Consequence: balance %+d, board %+d, supporters %+d, reputation %+d",
                            ev.option_balance_delta[i], ev.option_board_delta[i], ev.option_supporter_delta[i], ev.option_reputation_delta[i]);
            }
        } else {
            ImGui::Text("No active decision event today.");
        }

        ImGui::Spacing();
        ImGui::Text("Upcoming Fixtures");
        ImGui::Separator();
        for (const auto& fixture : upcoming_fixtures) {
            if (!fixture.played) render_fixture_row(fixture);
        }

        ImGui::Columns(1);
        ImGui::Separator();

        ImGui::Columns(2, "bottom_cols", true);

        ImGui::Text("Recent Results");
        ImGui::Separator();
        for (const auto& row : recent_results) {
            render_fixture_row(row);
        }

        ImGui::NextColumn();

        ImGui::Text("Recent Finance Entries");
        ImGui::Separator();
        for (const auto& e : finance_events) {
            ImGui::Text("%s [%s] %+d -> \xC2\xA3%d", e.event_date.c_str(), e.category.c_str(), e.amount, e.balance_after);
            ImGui::TextWrapped("%s", e.description.c_str());
        }

        ImGui::Spacing();
        ImGui::Text("Supporter Reactions");
        ImGui::Separator();
        for (const auto& note : supporter_notes) {
            ImGui::Text("%s  %+d  mood %d", note.note_date.c_str(), note.delta, note.mood_after);
            ImGui::TextWrapped("%s", note.reason.c_str());
        }

        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        int w = 0;
        int h = 0;
        SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.18f, 0.30f, 1.0f);
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
    return 0;
}
