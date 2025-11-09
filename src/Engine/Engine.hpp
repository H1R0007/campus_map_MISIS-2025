#pragma once
#include <SDL2/SDL.h>
#include "imgui.h"
#include <memory>

class MapViewer;  // forward-declared to reduce coupling
class UIManager;

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
    void drawLoadingScreen();

    MapViewer& getMapViewer() { return *mapViewer; }

private:
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window;
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer;
    std::unique_ptr<MapViewer> mapViewer;
    std::unique_ptr<UIManager> uiManager;

    bool isRunning;

    bool isLoading = true;
    float loadingTimer = 0.0f; // секунды от старта

    // Поля для хранения текущего состояния окна
    int currentWidth;
    int currentHeight;

    // input-processing + frame rendering
    void handleEvents();
    void render();

    // --- ImGui Helper Methods ---
    void initImGui();
    void shutdownImGui();

};