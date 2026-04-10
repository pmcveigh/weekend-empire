#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <iostream>
#include <string>

int main(int, char**) {
    std::cout << "[startup] Weekend Empire bootstrap starting" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[error] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

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
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "[error] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    std::cout << "[startup] Creating OpenGL context" << std::endl;
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::cerr << "[error] SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (SDL_GL_SetSwapInterval(1) != 0) {
        std::cerr << "[warn] VSync request failed: " << SDL_GetError() << std::endl;
    }

    std::cout << "[startup] OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "[runtime] Entering main loop" << std::endl;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                std::cout << "[event] Window close requested" << std::endl;
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    std::cout << "[event] Escape pressed, quitting" << std::endl;
                    running = false;
                } else if (key == SDLK_SPACE) {
                    std::cout << "[event] Space pressed" << std::endl;
                } else if (key == SDLK_F1) {
                    std::cout << "[event] F1 pressed" << std::endl;
                }
            }
        }

        glViewport(0, 0, 1280, 720);
        glClearColor(0.10f, 0.18f, 0.32f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    std::cout << "[shutdown] Cleaning up" << std::endl;
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[shutdown] Exit complete" << std::endl;
    return 0;
}
