#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <iostream>

namespace {

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

void apply_viewport(SDL_Window* window, bool log_resize) {
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);

    glViewport(0, 0, drawable_width, drawable_height);

    if (log_resize) {
        std::cout << "[render] Viewport set to " << drawable_width << "x" << drawable_height << std::endl;
    }
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

    std::cout << "[startup] OpenGL context created" << std::endl;

    if (SDL_GL_MakeCurrent(window, gl_context) != 0) {
        std::cerr << "[error] SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

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

    apply_viewport(window, true);

    std::cout << "[runtime] Entering main loop" << std::endl;

    bool running = true;
    Uint64 previous_counter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    double timing_log_accumulator = 0.0;

    while (running) {
        const Uint64 frame_start_counter = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                std::cout << "[event] Window close requested" << std::endl;
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    std::cout << "[event] Window resized to " << event.window.data1 << "x" << event.window.data2 << std::endl;
                    apply_viewport(window, true);
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
                } else if (key == SDLK_SPACE) {
                    std::cout << "[input] Space " << state << std::endl;
                } else if (key == SDLK_F1) {
                    std::cout << "[input] F1 " << state << std::endl;
                }
            }
        }

        const Uint64 now_counter = SDL_GetPerformanceCounter();
        const double delta_time = static_cast<double>(now_counter - previous_counter) / static_cast<double>(frequency);
        previous_counter = now_counter;

        glClearColor(0.10f, 0.18f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

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
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[shutdown] Exit complete" << std::endl;

    return 0;
}
