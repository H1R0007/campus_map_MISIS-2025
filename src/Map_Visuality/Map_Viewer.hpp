#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "../Camera/Camera.hpp"
#include "../path_finder/path_finder.hpp"
#include "../aliases/AliasManager.hpp"
#include "../map/GraphManager.hpp"

// === ViewMode ===
// Either entire campus view, or a specific building floor.
enum class ViewMode { Campus, BuildingFloor };

// === MapViewer ===
// Main visualization layer: renders campus/buildings/floors, handles input,
// overlays (search box, dev mode info), and user pathfinding.
class MapViewer {
public:
    MapViewer(SDL_Renderer* renderer, const char* mapPath);
    ~MapViewer();

    // === Lifecycle ===
    void onWindowResized(int w, int h);

    // === Rendering ===
    void render();         // draw map + dev layers + path
    void renderOverlay();  // UI overlays (scale, coords, search inputs, etc.)

    // === Events ===
    void handleEvent(SDL_Event& event);

    // === Pathfinding ===
    void buildPathFromAliases(const std::string& startName, const std::string& endName);
    void switchToFloor(const std::string& buildingId, int floor);

    // === Getters ===
    GraphManager& getGraphManager() { return graphManager; }

private:
    // === Core state ===
    GraphManager graphManager;
    AliasManager aliasManager;
    Camera camera;

    SDL_Texture* mapTexture = nullptr;
    SDL_Renderer* renderer;
    SDL_Point mapSize;
    SDL_Rect mapRect;
    SDL_Point lastMousePos;
    SDL_Point debugMouseWorld{ 0,0 };
    TTF_Font* font = nullptr;

    // === Path selection ===
    std::string startNodeId;
    std::string endNodeId;
    std::vector<std::string> currentPath;

    // === Neighbor editing (dev mode) ===
    std::string activeNodeId;
    bool neighborMode = false;
    std::vector<std::string> pendingNeighbors;
    std::string portalStartNode;

    // === Overlay UI ===
    std::string inputFrom;
    std::string inputTo;
    bool editingFrom = true; // true = redact "OTKYDA", false = redact "KYDA"
    int selectedSuggestionIndex = -1;
    std::vector<std::string> currentSuggestions;
    Uint32 lastSaveTick = 0;

    // === View control ==
    ViewMode currentView = ViewMode::Campus;
    std::string currentBuilding;
    int currentFloor = 0;

    // User options
    bool userAllowStairs = true;
    bool userAllowLift = true;
    bool userAllowBridge = true;

    // Dev inspector
    bool debugDrawNodes = true;
    const Node* hoveredNode = nullptr;
    std::string inspectorNodeId;

    // === Private helpers ===
    void loadMap(const char* path);

    // Helper: find node under cursor screen coordinates. 
    // Returns empty string if nothing found.
    std::string findNodeUnderCursor(const SDL_Point& clickScreen, int radius = 25) const {
        const auto& nodesHere =
            (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
            : graphManager.getActiveNodes();
        for (auto& [id, node] : nodesHere) {
            SDL_Point scr = camera.worldToScreen({ node.x, node.y });
            int dx = scr.x - clickScreen.x;
            int dy = scr.y - clickScreen.y;
            if (dx * dx + dy * dy <= radius * radius) {
                return id;
            }
        }
        return "";
    }

    // === Input fields (route search) ===
    SDL_Rect fromFieldRect{};   // hitbox of "OTKYDA" field
    SDL_Rect toFieldRect{};     // hitbox of "KYDA" field
    bool inputActive = false;   // true if any input field focused

    // === Create nodes in line ===
    void createNodeLine(const SDL_Point& startWorld, float angleDeg, int count, int step);
    bool lineToolActive = false;
    bool lineToolFirstPointSet = false;
    SDL_Point lineToolStart{};
    SDL_Point lineToolEnd{};
    float lineToolAngleDeg = 0.0f;
    float lineToolDistance = 0.0f;
    int lineToolCount = 5;    // сколько узлов (по умолчанию)
    int lineToolStep = 100;   // если count не задан, ориентируемся по step
    bool lineToolReady = false;  // флаг готовности построить
};