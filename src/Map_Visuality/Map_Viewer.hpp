#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "../Camera/Camera.hpp"
#include "../map/Graph.hpp"

class MapViewer {
public:
    MapViewer(SDL_Renderer* renderer, const char* mapPath);
    ~MapViewer();

    void handleEvent(SDL_Event& event);
    void render();
    void renderOverlay();  // текст поверх
    void onWindowResized(int w, int h);

private:

    Graph graph;

    bool debugDrawNodes = true;
    const Node* hoveredNode = nullptr;

    std::string activeNodeId;
    bool neighborMode = false;
    std::vector<std::string> pendingNeighbors;  // временные соседи

    SDL_Texture* mapTexture;
    SDL_Renderer* renderer;
    Camera camera;
    SDL_Point mapSize;
    SDL_Rect mapRect;
    SDL_Point lastMousePos;
    SDL_Point debugMouseWorld{ 0,0 };

    TTF_Font* font = nullptr;

    void loadMap(const char* path);

    Uint32 lastSaveTick = 0;
};