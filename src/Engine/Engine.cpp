#include "Engine.hpp"
#include "../config.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include <SDL2/SDL_ttf.h>
#include <iostream>

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

void Engine::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) stop();

        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_F11) {
                Uint32 flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    SDL_SetWindowFullscreen(window, 0);  // выйти из fullscreen
                }
                else {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP); // войти
                }
            }
            if (event.key.keysym.sym == SDLK_F2) {
                // Эта клавиша будет работать только если билд разработческий
                if (Config::BUILD_DEV) {
                    Config::toggleDevMode();
                    std::cout << "Switched mode: " << (Config::DEV_MODE ? "DEV" : "USER") << "\n";
                }
            }
        }

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

void Engine::render() {
    // белый фон
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    mapViewer->render();      // карта

    // рисуем overlay — процент масштаба
    mapViewer->renderOverlay();

    SDL_RenderPresent(renderer);
}

void Engine::run() {
    while (isRunning) {
        handleEvents();
        render();
        SDL_Delay(16);
    }
}

void Engine::handleFrame() {
    handleEvents();
    render();
    // без while и без SDL_Delay
}

Engine::~Engine() {
    // Сохраняем всё перед выходом (только в DEV_MODE)
    if (Config::DEV_MODE && mapViewer) {
        // Сохраним активный граф (этаж/кампус)
        mapViewer->getGraphManager().saveActive();

        // Сохраним переходы (порталы/лестницы/двери)
        mapViewer->getGraphManager().saveTransitions("assets/transitions/transitions.json");

        std::cout << "[Engine] Автоматически сохранены графы и переходы перед выходом\n";
    }

    delete mapViewer;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_StopTextInput();
    TTF_Quit();
    SDL_Quit();
}