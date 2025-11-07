#pragma once
#include <SDL2/SDL.h>
#include <memory>

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
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer;
    std::unique_ptr<MapViewer> mapViewer;

    bool isRunning;

    // ѕол€ дл€ хранени€ текущего состо€ни€ окна
    int currentWidth;
    int currentHeight;

    // input-processing + frame rendering
    void handleEvents();
    void render();
};