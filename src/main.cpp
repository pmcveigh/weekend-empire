#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <sqlite3.h>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <iostream>
#include <string>

namespace {

struct ClubOverview {
    std::string club_name;
    std::string league_name;
    int balance = 0;
    int wage_bill = 0;
    std::string season_phase;
    int fixture_count = 0;
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

bool open_database(sqlite3** database, const char* path) {
    std::cout << "[startup] Initialising SQLite" << std::endl;
    if (sqlite3_open(path, database) != SQLITE_OK) {
        std::cerr << "[error] sqlite3_open failed: " << sqlite3_errmsg(*database) << std::endl;
        return false;
    }
    std::cout << "[startup] SQLite opened at " << path << std::endl;
    return true;
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

bool ensure_schema(sqlite3* database) {
    const char* schema_sql = R"SQL(
CREATE TABLE IF NOT EXISTS leagues (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS clubs (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    league_id INTEGER NOT NULL,
    balance INTEGER NOT NULL,
    wage_bill INTEGER NOT NULL,
    season_phase TEXT NOT NULL,
    fixture_count INTEGER NOT NULL,
    FOREIGN KEY (league_id) REFERENCES leagues(id)
);
)SQL";

    return run_sql(database, schema_sql, "Schema verification");
}

bool ensure_sample_data(sqlite3* database) {
    const char* sample_sql = R"SQL(
INSERT OR IGNORE INTO leagues (id, name)
VALUES (1, 'Northern Premier Prototype');

INSERT OR IGNORE INTO clubs (id, name, league_id, balance, wage_bill, season_phase, fixture_count)
VALUES (1, 'Bangor FC', 1, 250000, 18000, 'Pre-Season', 0);
)SQL";

    return run_sql(database, sample_sql, "Sample data verification");
}

bool load_club_overview(sqlite3* database, ClubOverview* overview) {
    const char* query = R"SQL(
SELECT
    c.name,
    l.name,
    c.balance,
    c.wage_bill,
    c.season_phase,
    c.fixture_count
FROM clubs c
JOIN leagues l ON l.id = c.league_id
ORDER BY c.id
LIMIT 1;
)SQL";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, query, -1, &statement, nullptr) != SQLITE_OK) {
        std::cerr << "[error] sqlite3_prepare_v2 failed: " << sqlite3_errmsg(database) << std::endl;
        return false;
    }

    bool loaded = false;
    const int step_result = sqlite3_step(statement);
    if (step_result == SQLITE_ROW) {
        const unsigned char* club_name_text = sqlite3_column_text(statement, 0);
        const unsigned char* league_name_text = sqlite3_column_text(statement, 1);
        const unsigned char* season_phase_text = sqlite3_column_text(statement, 4);

        overview->club_name = club_name_text ? reinterpret_cast<const char*>(club_name_text) : "Unknown Club";
        overview->league_name = league_name_text ? reinterpret_cast<const char*>(league_name_text) : "Unknown League";
        overview->balance = sqlite3_column_int(statement, 2);
        overview->wage_bill = sqlite3_column_int(statement, 3);
        overview->season_phase = season_phase_text ? reinterpret_cast<const char*>(season_phase_text) : "Unknown";
        overview->fixture_count = sqlite3_column_int(statement, 5);

        std::cout << "[runtime] Loaded UI data from SQLite: " << overview->club_name << std::endl;
        loaded = true;
    } else {
        std::cerr << "[warn] No club rows available for UI" << std::endl;
    }

    sqlite3_finalize(statement);
    return loaded;
}

}

int main(int, char**) {
    std::cout << "[startup] Weekend Empire starting" << std::endl;
    std::cout << "[startup] Initialising SDL video/events/timer" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[error] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "[startup] SDL initialised" << std::endl;

    if (!configure_sdl_gl_attributes()) {
        SDL_Quit();
        return 1;
    }

    std::cout << "[startup] Creating SDL window" << std::endl;
    SDL_Window* window = SDL_CreateWindow(
        "Weekend Empire",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "[error] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    std::cout << "[startup] Window created" << std::endl;

    std::cout << "[startup] Creating OpenGL context" << std::endl;
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "[error] SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "[error] SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "[startup] OpenGL context created" << std::endl;

    int context_major = 0;
    int context_minor = 0;
    int context_profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &context_major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &context_minor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &context_profile);
    std::cout << "[startup] Active OpenGL context: " << context_major << "." << context_minor
              << " profile mask=" << context_profile << std::endl;

    if (SDL_GL_SetSwapInterval(1) != 0) {
        std::cerr << "[warn] VSync request failed: " << SDL_GetError() << std::endl;
    } else {
        std::cout << "[startup] VSync enabled" << std::endl;
    }

    log_gl_string("OpenGL vendor", GL_VENDOR);
    log_gl_string("OpenGL renderer", GL_RENDERER);
    log_gl_string("OpenGL version", GL_VERSION);

    std::cout << "[startup] Initialising Dear ImGui" << std::endl;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)) {
        std::cerr << "[error] ImGui_ImplSDL2_InitForOpenGL failed" << std::endl;
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "[error] ImGui_ImplOpenGL3_Init failed" << std::endl;
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "[startup] Dear ImGui initialised" << std::endl;

    sqlite3* database = nullptr;
    if (!open_database(&database, "weekend_empire.db")) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!ensure_schema(database) || !ensure_sample_data(database)) {
        sqlite3_close(database);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ClubOverview club_overview;
    if (!load_club_overview(database, &club_overview)) {
        club_overview = {"Unknown Club", "Unknown League", 0, 0, "Unknown", 0};
    }

    std::cout << "[runtime] Entering main loop" << std::endl;

    bool running = true;
    Uint64 previous_counter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    double timing_log_accumulator = 0.0;
    std::string last_action = "None";

    while (running) {
        const Uint64 frame_start_counter = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                std::cout << "[event] Window close requested" << std::endl;
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    std::cout << "[event] Window resized to " << event.window.data1 << "x" << event.window.data2 << std::endl;
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    std::cout << "[event] Window focus gained" << std::endl;
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    std::cout << "[event] Window focus lost" << std::endl;
                }
            } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                const bool pressed = event.type == SDL_KEYDOWN;
                if (pressed && event.key.repeat != 0) {
                    continue;
                }

                const SDL_Keycode key = event.key.keysym.sym;
                const char* state = pressed ? "down" : "up";

                if (key == SDLK_ESCAPE) {
                    std::cout << "[input] Escape " << state << std::endl;
                    if (pressed) {
                        std::cout << "[event] Escape pressed, quitting" << std::endl;
                        running = false;
                    }
                }
            }
        }

        const Uint64 now_counter = SDL_GetPerformanceCounter();
        const double delta_time = static_cast<double>(now_counter - previous_counter) / static_cast<double>(frequency);
        previous_counter = now_counter;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(450.0f, 320.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Weekend Empire");
        ImGui::Text("Chairman Prototype Dashboard");
        ImGui::Separator();
        ImGui::Text("Club: %s", club_overview.club_name.c_str());
        ImGui::Text("League: %s", club_overview.league_name.c_str());
        ImGui::Text("Balance: \xC2\xA3%d", club_overview.balance);
        ImGui::Text("Wage Bill: \xC2\xA3%d p/w", club_overview.wage_bill);
        ImGui::Text("Season Phase: %s", club_overview.season_phase.c_str());
        ImGui::Text("Fixture Count: %d", club_overview.fixture_count);
        ImGui::Separator();

        if (ImGui::Button("Advance Day")) {
            last_action = "Advance Day pressed";
            std::cout << "[ui] Advance Day pressed" << std::endl;
        }

        ImGui::SameLine();

        if (ImGui::Button("Refresh Club Data")) {
            if (load_club_overview(database, &club_overview)) {
                last_action = "Refresh Club Data pressed";
            } else {
                last_action = "Refresh Club Data pressed (no data)";
            }
            std::cout << "[ui] Refresh Club Data pressed" << std::endl;
        }

        ImGui::Text("Last Action: %s", last_action.c_str());
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

        timing_log_accumulator += delta_time;
        if (timing_log_accumulator >= 1.0) {
            const double frame_ms = delta_time * 1000.0;
            const double fps = delta_time > 0.0 ? 1.0 / delta_time : 0.0;
            std::cout << "[timing] dt=" << frame_ms << " ms, fps=" << fps << std::endl;
            timing_log_accumulator = 0.0;
        }

        const Uint64 frame_end_counter = SDL_GetPerformanceCounter();
        const double frame_seconds = static_cast<double>(frame_end_counter - frame_start_counter) / static_cast<double>(frequency);
        if (frame_seconds < 0.001) {
            SDL_Delay(1);
        }
    }

    std::cout << "[shutdown] Cleaning up" << std::endl;
    sqlite3_close(database);
    std::cout << "[shutdown] SQLite closed" << std::endl;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    std::cout << "[shutdown] Dear ImGui shut down" << std::endl;
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[shutdown] Exit complete" << std::endl;

    return 0;
}
