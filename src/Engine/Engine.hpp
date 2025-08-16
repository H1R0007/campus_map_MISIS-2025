#pragma once
#include <SDL2/SDL.h>

class MapViewer;  // Forward declaration

class Engine {
public:
    Engine(const char* title, int width, int height);
    ~Engine();

    void run();
    void stop() { isRunning = false; }  // Реализация inline

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool isRunning;
    MapViewer* mapViewer;

    void handleEvents();
    void render();
};