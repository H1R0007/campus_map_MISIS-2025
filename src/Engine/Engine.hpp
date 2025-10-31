#pragma once
#include <SDL2/SDL.h>

class MapViewer;  // forward-declared to reduce coupling

// === Engine ===
// Main SDL runtime controller. Owns window, renderer, event loop,
// and delegates rendering to MapViewer.
class Engine {
public:
    Engine(const char* title, int width, int height);
    ~Engine();

    void run();              // desktop loop
    void handleFrame();      // wasm loop (called each frame)
    void stop() { isRunning = false; }

    MapViewer& getMapViewer() { return *mapViewer; }

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool isRunning;
    MapViewer* mapViewer;

    // input-processing + frame rendering
    void handleEvents();
    void render();
};