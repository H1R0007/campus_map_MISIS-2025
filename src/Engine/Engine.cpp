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

    window = SDL_CreateWindow(
        Config::WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Config::WINDOW_WIDTH,
        Config::WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    mapViewer = new MapViewer(renderer, Config::MAP_PATH);
}

void Engine::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) stop();
        mapViewer->handleEvent(event);
    }
}

void Engine::render() {
    // белый фон
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    mapViewer->render();      // карта

    // рисуем overlay Ч процент масштаба
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

Engine::~Engine() {
    delete mapViewer;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}