#include "Engine.hpp"
#include "../config.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include <SDL2/SDL_ttf.h>
#include <iostream>

// --- Lifecycle ---
Engine::Engine(const char* title, int w, int h)
    : window(nullptr, SDL_DestroyWindow),       // »нициализируем умные указатели
    renderer(nullptr, SDL_DestroyRenderer),
    isRunning(true),
    currentWidth(Config::INITIAL_WINDOW_WIDTH),
    currentHeight(Config::INITIAL_WINDOW_HEIGHT)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        throw std::runtime_error("SDL Init failed");
    }

    if (TTF_Init() == -1) {
        std::cerr << "Failed to init TTF: " << TTF_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("TTF Init failed");
    }

    SDL_StartTextInput();

    // »спользуем .reset() дл€ присваивани€ значени€
    window.reset(SDL_CreateWindow(
        Config::WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        currentWidth,
        currentHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    ));
    if (!window) {
        throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
    }

    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
    if (!renderer) {
        throw std::runtime_error("SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
    }

    // `std::make_unique` - безопасный способ создани€
    mapViewer = std::make_unique<MapViewer>(renderer.get(), Config::CAMPUS_MAP_PATH);
}

Engine::~Engine() {
    SDL_StopTextInput();
    if (TTF_WasInit()) TTF_Quit();
    SDL_Quit();
}

// --- Run loops ---
void Engine::run() {
    const Uint32 frameDelay = 16;
    while (isRunning) {
        Uint32 start = SDL_GetTicks();

        handleEvents();
        render();

        Uint32 frameTime = SDL_GetTicks() - start;
        if (frameDelay > frameTime)
            SDL_Delay(frameDelay - frameTime);
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
            Uint32 flags = SDL_GetWindowFlags(window.get());
            SDL_SetWindowFullscreen(window.get(),
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
            currentWidth = event.window.data1;
            currentHeight = event.window.data2;

            // —ообщаем MapViewer об изменении размера
            mapViewer->onWindowResized(currentWidth, currentHeight);
        }

        mapViewer->handleEvent(event);
    }
}

// --- Rendering ---
void Engine::render() {
    // white
    SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
    SDL_RenderClear(renderer.get());                         

    mapViewer->render();      // map
    mapViewer->renderOverlay();

    SDL_RenderPresent(renderer.get());           
}
