#pragma once
#include <SDL2/SDL_image.h>
#include <string>

// Forward-declare
class MapViewer;

class UIManager {
public:
    UIManager();

    // The main function to draw the entire UI for a frame.
    // It takes a reference to MapViewer to get data and call its methods.
    void render(MapViewer& mapViewer);

    void setLogoTexture(SDL_Texture* tex) { logoTexture = tex; }
private:
    struct AutoCompleteState {
        bool active = false;
        bool editingFrom = true;
        int hovered = -1;
    } autoState;

    // Локальное состояние UI
    SDL_Texture* logoTexture = nullptr;

    // Состояние объединённой панели маршрута
    bool routePanelExpanded = false;   // Свернуто по умолчанию (только "Откуда")

    // Dev dock state (DEV only)
    bool devToolsVisible = true;
    bool devToolsMini = false;
    bool devDemoOpen = false; // опционально ImGui Demo

    // Private methods to draw specific UI windows for better organization
    void drawSearchWindow(MapViewer& mapViewer); // объединённая панель
    void drawDevInfoWindow(MapViewer& mapViewer);
    void drawTopNavBar(MapViewer& viewer);
    void drawRightPanel(MapViewer& viewer);
    void drawFloorBuildingPanel(MapViewer& mapViewer);
    void drawBottomMenuBar(MapViewer& viewer);

    // New: compact developer dock + its always-available toggle button
    void drawDevDock(MapViewer& viewer);
    void drawDevDockToggleButton();
};