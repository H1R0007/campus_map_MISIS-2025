#pragma once
#include <SDL2/SDL_image.h>
#include "imgui.h"
#include <string>
#include <vector>

class MapViewer;

class UIManager {
public:
    UIManager();

    void render(MapViewer& mapViewer);
    void setLogoTexture(SDL_Texture* tex) { logoTexture = tex; }

private:
    struct AutoCompleteState {
        bool active = false;
        bool editingFrom = true;
        int hovered = -1;
    } autoState;

    // UI state
    SDL_Texture* logoTexture = nullptr;

    // Route panel state
    bool routePanelExpanded = false;

    // Dev dock state (DEV only)
    bool devToolsVisible = true;
    bool devToolsMini = false;
    bool devDemoOpen = false;

    // Toasts (ненавязчивая обратная связь)
    struct Toast {
        std::string text;
        ImVec4 color;
        float ttl;   // сек
        float age;   // сек
    };
    std::vector<Toast> toasts;
    void pushToast(const std::string& txt, ImVec4 col = ImVec4(0.06f, 0.60f, 1.0f, 1.0f), float duration = 1.8f);
    void drawToasts();

    // Windows
    void drawSearchWindow(MapViewer& mapViewer);
    void drawTopNavBar(MapViewer& viewer);
    void drawFloorBuildingPanel(MapViewer& mapViewer);
    void drawBottomMenuBar(MapViewer& viewer);
    void drawLogoOverlay();

    // Dev Dock
    void drawDevDock(MapViewer& viewer);
};