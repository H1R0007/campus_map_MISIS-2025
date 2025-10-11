#include "Engine.hpp"
#include "../config.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include <SDL2/SDL_ttf.h>
#include <iostream>

// --- Lifecycle ---
Engine::Engine(const char* title, int w, int h) : isRunning(true) {
    SDL_Init(SDL_INIT_VIDEO);

    if (TTF_Init() == -1) {
        std::cerr << "Failed to init TTF: " << TTF_GetError() << std::endl;
    }

    SDL_StartTextInput();

    window = SDL_CreateWindow(
        Config::WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Config::WINDOW_WIDTH,
        Config::WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    mapViewer = new MapViewer(renderer, Config::CAMPUS_MAP_PATH);
}

Engine::~Engine() {
    delete mapViewer;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_StopTextInput();

    TTF_Quit();
    SDL_Quit();
}

// --- Run loops ---
void Engine::run() {
    while (isRunning) {
        handleEvents();
        render();
        SDL_Delay(16);  // ~60 FPS throttle on desktop
    }
}

void Engine::handleFrame() {
    // Called by emscripten main loop -> single frame only
    handleEvents();
    render();
}

// --- Event handling ---
void Engine::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) stop();

        // Window fullscreen toggle
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
            Uint32 flags = SDL_GetWindowFlags(window);
            SDL_SetWindowFullscreen(window,
                (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        // DEV mode toggle
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2) {
            if (Config::BUILD_DEV) {
                Config::toggleDevMode();
                std::cout << "Switched mode: "
                    << (Config::DEV_MODE ? "DEV" : "USER") << "\n";
            }
        }
        // Window resize -> propagate to camera
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            int newW = event.window.data1;
            int newH = event.window.data2;
            Config::WINDOW_WIDTH = newW;
            Config::WINDOW_HEIGHT = newH;

            // Сообщаем всем заинтересованным
            // (Можно напрямую обновить камеру через MapViewer)
            mapViewer->onWindowResized(newW, newH);
        }

        mapViewer->handleEvent(event);
    }
}

// --- Rendering ---
void Engine::render() {
    // white
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    mapViewer->render();      // map

    mapViewer->renderOverlay();

    SDL_RenderPresent(renderer);
}
