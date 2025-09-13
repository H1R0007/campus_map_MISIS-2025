#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "../Camera/Camera.hpp"
#include "../path_finder/path_finder.hpp"
#include "../aliases/AliasManager.hpp"
#include "../map/GraphManager.hpp"

enum class ViewMode { Campus, BuildingFloor };

class MapViewer {
public:
    MapViewer(SDL_Renderer* renderer, const char* mapPath);
    GraphManager& getGraphManager() { return graphManager; }
    ~MapViewer();

    void handleEvent(SDL_Event& event);
    void render();
    void renderOverlay();  // текст поверх
    void onWindowResized(int w, int h);

    void buildPathFromAliases(const std::string& startName, const std::string& endName);
    void switchToFloor(const std::string& buildingId, int floor);

private:
    GraphManager graphManager;

    bool debugDrawNodes = true;
    const Node* hoveredNode = nullptr;

    std::string startNodeId;
    std::string endNodeId;
    std::vector<std::string> currentPath;

    std::string activeNodeId;
    bool neighborMode = false;
    std::vector<std::string> pendingNeighbors;

    std::string portalStartNode;

    SDL_Texture* mapTexture = nullptr;
    SDL_Renderer* renderer;
    Camera camera;
    SDL_Point mapSize;
    SDL_Rect mapRect;
    SDL_Point lastMousePos;
    SDL_Point debugMouseWorld{ 0,0 };

    TTF_Font* font = nullptr;

    void loadMap(const char* path);

    Uint32 lastSaveTick = 0;

    AliasManager aliasManager;

    std::string inputFrom;
    std::string inputTo;
    bool editingFrom = true;

    int selectedSuggestionIndex = -1;
    std::vector<std::string> currentSuggestions;

    ViewMode currentView = ViewMode::Campus;
    std::string currentBuilding;
    int currentFloor = 0;

    bool userAllowStairs = true;
    bool userAllowLift = true;
    bool userAllowBridge = true;

    // ===== NEW: LineMode =====
    bool lineMode = false;         // активен ли режим линии
    bool lineStartSet = false;     // выбрана ли уже первая точка
    SDL_Point lineStart;           // первая точка линии
};