#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <iostream>
#include <string>

int main(int, char**) {
    std::cout << "[startup] Weekend Empire platform bootstrap starting" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[error] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::cout << "[startup] SDL initialised" << std::endl;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

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

    if (SDL_GL_SetSwapInterval(1) != 0) {
        std::cerr << "[warn] VSync request failed: " << SDL_GetError() << std::endl;
    } else {
        std::cout << "[startup] VSync enabled" << std::endl;
    }

    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        std::cout << "[startup] OpenGL version: " << version << std::endl;
    } else {
        std::cout << "[warn] OpenGL version string unavailable" << std::endl;
    }

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
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
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

        int drawable_width = 0;
        int drawable_height = 0;
        SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);

        glViewport(0, 0, drawable_width, drawable_height);
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
