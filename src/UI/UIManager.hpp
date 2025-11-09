#pragma once
#include <SDL2/SDL_image.h>

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

    SDL_Texture* logoTexture = nullptr;

    // Private methods to draw specific UI windows for better organization
    void drawSearchWindow(MapViewer& mapViewer);
    void drawDevInfoWindow(MapViewer& mapViewer);
    void drawTopNavBar(MapViewer& viewer);
    void drawLeftSidebar(MapViewer& viewer);
    void drawRightPanel(MapViewer& viewer);
    void drawFloorBuildingPanel(MapViewer& mapViewer);
};