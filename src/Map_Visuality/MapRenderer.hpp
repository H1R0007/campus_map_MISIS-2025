#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <memory>

// Вперед объявляем классы
class Camera;
class GraphManager;
struct Node;

class MapRenderer {
public:
    MapRenderer(SDL_Renderer* renderer);
    // SDL_Renderer* getRenderer() const { return renderer; }
    ~MapRenderer();

    bool loadMapTexture(const std::string& path);

    // Отрисовка основной сцены (карта, сетка, узлы, пути)
    void renderScene(
        const Camera& camera,
        const GraphManager& graphManager,
        const std::string& currentView, // "campus" or "floor"
        int currentFloor,
        const std::string& currentBuilding,
        const std::vector<std::string>& currentPath,
        const Node* hoveredNode,
        const std::string& activeNodeId,
        const std::vector<std::string>& pendingNeighbors,
        const std::string& portalStartNode,
        bool devMode,
        bool debugDrawNodes,
        bool neighborMode,
        bool lineToolActive,
        const SDL_Point& lineToolStart,
        const SDL_Point& lineToolEnd
    );

    // Хелпер для получения размеров отрисованного текста (нужен для InputHandler)
    SDL_Rect getTextSize(const std::string& text);
    SDL_Renderer* getRenderer() const { return renderer; }

private:
    SDL_Renderer* renderer;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> mapTexture;
    std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> font;
    SDL_Point mapSize;

    // Приватный хелпер для отрисовки текста
    void drawText(const std::string& text, int x, int y, SDL_Color color);
};