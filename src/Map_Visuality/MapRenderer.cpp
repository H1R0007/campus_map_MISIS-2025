#include "MapRenderer.hpp"
#include "../config.hpp"
#include "../Camera/Camera.hpp"
#include "../map/GraphManager.hpp"
#include <iostream>

MapRenderer::MapRenderer(SDL_Renderer* renderer)
    : renderer(renderer),
    mapTexture(nullptr, SDL_DestroyTexture),
    font(nullptr, TTF_CloseFont),
    mapSize{ 0, 0 }
{
    font.reset(TTF_OpenFont(Config::FONT_PATH, 16));
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
    }
}

MapRenderer::~MapRenderer() {}

bool MapRenderer::loadMapTexture(const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Failed to load map: " << path << " | SDL_image error: " << IMG_GetError() << std::endl;
        return false;
    }

    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!newTexture) {
        std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return false;
    }

    mapTexture.reset(newTexture);
    mapSize.x = surface->w;
    mapSize.y = surface->h;
    SDL_FreeSurface(surface);
    return true;
}

void MapRenderer::renderScene(
    const Camera& camera, 
    const GraphManager& graphManager, 
    const std::string& currentView,
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
    const SDL_Point& lineToolEnd)
{
    // 1. Отрисовка карты
    if (mapTexture) {
        SDL_Rect mapWorldRect;
        mapWorldRect.w = this->mapSize.x;
        mapWorldRect.h = this->mapSize.y;
        mapWorldRect.x = (Config::CANVAS_WIDTH - mapWorldRect.w) / 2;
        mapWorldRect.y = (Config::CANVAS_HEIGHT - mapWorldRect.h) / 2;

        SDL_Rect viewportWorld = camera.getViewport();
        SDL_Rect mapInWorldScaled = {
            static_cast<int>(mapWorldRect.x * camera.getScale()), static_cast<int>(mapWorldRect.y * camera.getScale()),
            static_cast<int>(mapWorldRect.w * camera.getScale()), static_cast<int>(mapWorldRect.h * camera.getScale())
        };

        SDL_Rect intersection;
        if (SDL_IntersectRect(&viewportWorld, &mapInWorldScaled, &intersection)) {
            SDL_Rect srcRect, destRect;
            srcRect.x = static_cast<int>((intersection.x - mapInWorldScaled.x) / camera.getScale());
            srcRect.y = static_cast<int>((intersection.y - mapInWorldScaled.y) / camera.getScale());
            srcRect.w = std::max(1, static_cast<int>(intersection.w / camera.getScale()));
            srcRect.h = std::max(1, static_cast<int>(intersection.h / camera.getScale()));

            destRect.x = intersection.x - viewportWorld.x;
            destRect.y = intersection.y - viewportWorld.y;
            destRect.w = intersection.w;
            destRect.h = intersection.h;

            SDL_RenderCopy(renderer, mapTexture.get(), &srcRect, &destRect);
        }
    }

    const auto& nodesToRender = (currentView == "campus")
        ? graphManager.getCampusNodes()
        : graphManager.getActiveNodes();

    // 2. Отрисовка DEV-элементов (сетка, узлы, рёбра)
#ifndef __EMSCRIPTEN__
    if (devMode) {
        // Отрисовка сетки
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        int winW, winH;
        SDL_GetRendererOutputSize(renderer, &winW, &winH);
        SDL_Point topLeftWorld = camera.screenToWorld({ 0, 0 });
        SDL_Point botRightWorld = camera.screenToWorld({ winW, winH });
        int step = 100;
        while (step * camera.getScale() < 25) { step *= 2; }
        int startX = (topLeftWorld.x / step) * step;
        int startY = (topLeftWorld.y / step) * step;
        for (int x = startX; x <= botRightWorld.x; x += step) {
            SDL_Point scrA = camera.worldToScreen({ x, topLeftWorld.y });
            SDL_Point scrB = camera.worldToScreen({ x, botRightWorld.y });
            SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
        }
        for (int y = startY; y <= botRightWorld.y; y += step) {
            SDL_Point scrA = camera.worldToScreen({ topLeftWorld.x, y });
            SDL_Point scrB = camera.worldToScreen({ botRightWorld.x, y });
            SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
        }

        // Отрисовка рёбер
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        for (const auto& [id, node] : nodesToRender) {
            SDL_Point src = camera.worldToScreen({ node.x, node.y });
            for (const auto& nbId : node.neighbors) {
                const Node* nbPtr = graphManager.getNode(nbId);
                if (nbPtr && id < nbId) { // id < nbId чтобы не рисовать ребро дважды
                    SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                    SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                }
            }
        }

        // Рёбра в режиме добавления соседей
        if (neighborMode && !activeNodeId.empty()) {
            const Node* activeNode = graphManager.getNode(activeNodeId);
            if (activeNode) {
                SDL_Point src = camera.worldToScreen({ activeNode->x, activeNode->y });
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
                for (const auto& nbId : pendingNeighbors) {
                    const Node* nbPtr = graphManager.getNode(nbId);
                    if (nbPtr) {
                        SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                        SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                    }
                }
            }
        }

        // Подсветка начального узла для создания портала
        if (!portalStartNode.empty()) {
            const Node* p = graphManager.getNode(portalStartNode);
            if (p) {
                SDL_Point scr = camera.worldToScreen({ p->x, p->y });
                SDL_SetRenderDrawColor(renderer, 0, 200, 200, 255);
                SDL_Rect border{ scr.x - 6, scr.y - 6, 12, 12 };
                SDL_RenderDrawRect(renderer, &border);
            }
        }

        // Отрисовка узлов
        if (debugDrawNodes) {
            for (const auto& [id, node] : nodesToRender) {
                SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                SDL_Rect rect{ scr.x - 3, scr.y - 3, 6, 6 };
                if (neighborMode && !activeNodeId.empty() && id == activeNodeId) {
                    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // Активный для соседей - зеленый
                }
                else {
                    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // Остальные - красные
                }
                SDL_RenderFillRect(renderer, &rect);

                // Подсветка при наведении (hover)
                if (hoveredNode && hoveredNode->id == id) {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
                    SDL_Rect border{ scr.x - 5, scr.y - 5, 10, 10 };
                    SDL_RenderDrawRect(renderer, &border);
                    drawText(id, scr.x + 10, scr.y - 10, { 0, 0, 0, 255 });
                }
            }
        }
    }
#endif

    // 3. Отрисовка построенного пути
    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // Синий цвет для пути
        for (size_t i = 1; i < currentPath.size(); i++) {
            const Node* from = graphManager.getNode(currentPath[i - 1]);
            const Node* to = graphManager.getNode(currentPath[i]);
            if (!from || !to) continue;

            bool shouldDraw = false;
            if (currentView == "campus") {
                if (from->building == "CAMPUS" && to->building == "CAMPUS") shouldDraw = true;
            }
            else { // BuildingFloor
                if (from->building == currentBuilding && to->building == currentBuilding && from->floor == currentFloor) shouldDraw = true;
            }

            if (shouldDraw) {
                SDL_Point scrA = camera.worldToScreen({ from->x, from->y });
                SDL_Point scrB = camera.worldToScreen({ to->x, to->y });
                SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
            }
        }
    }
}

void MapRenderer::renderUI(
    const Camera& camera, 
    const GraphManager& graphManager, 
    const SDL_Point& debugMouseWorld,
    const std::string& inputFrom, 
    const std::string& inputTo, 
    bool editingFrom,
    const std::vector<std::string>& suggestions, 
    int selectedSuggestion,
    bool userAllowStairs, 
    bool userAllowLift, 
    bool userAllowBridge,
    const std::string& neighborModeActiveId, 
    const std::string& inspectorNodeId,
    Uint32 lastSaveTick, 
    bool devMode)
{
    if (!font) return;

    // --- Отладочная информация (масштаб, координаты) ---
    int percent = static_cast<int>(camera.getScale() * 100);
    drawText(std::to_string(percent) + "%", 10, 10, { 0, 0, 0, 255 });
#ifndef __EMSCRIPTEN__
    if (devMode) {
        drawText("X=" + std::to_string(debugMouseWorld.x) + " Y=" + std::to_string(debugMouseWorld.y), 10, 30, { 0, 0, 0, 255 });
    }
#endif
    // --- Поля ввода "Откуда" и "Куда" ---
    std::string labelFrom = "OTKYDA: " + inputFrom + (editingFrom ? "_" : "");
    drawText(labelFrom, 10, 60, { 0, 0, 0, 255 });
    std::string labelTo = "KYDA: " + inputTo + (!editingFrom ? "_" : "");
    drawText(labelTo, 10, 90, { 0, 0, 0, 255 });

    // --- Подсказки для автодополнения ---
    int winW, winH;
    SDL_GetRendererOutputSize(renderer, &winW, &winH);
    int baseX = winW - 300; // Немного левее
    int baseY = 50;
    for (int i = 0; i < (int)suggestions.size(); ++i) {
        SDL_Color color = (i == selectedSuggestion) ? SDL_Color{ 200, 0, 0, 255 } : SDL_Color{ 0, 0, 200, 255 };
        drawText(suggestions[i], baseX, baseY + i * 20, color);
    }

    // --- Опции пользователя (лестницы, лифты) ---
    std::string optsText = "Options: [8]Stairs=" + std::string(userAllowStairs ? "ON" : "OFF") +
        " | [9]Lift=" + std::string(userAllowLift ? "ON" : "OFF") +
        " | [0]Bridge=" + std::string(userAllowBridge ? "ON" : "OFF");
    SDL_Rect optsSize = getTextSize(optsText);
    drawText(optsText, winW - optsSize.w - 10, winH - optsSize.h - 10, { 50, 50, 50, 255 });

#ifndef __EMSCRIPTEN__
    if (!devMode) return; // Остальное - только для DEV режима

    // --- Сообщение о режиме добавления соседей ---
    if (!neighborModeActiveId.empty()) {
        drawText("Adding neighbors for: " + neighborModeActiveId, 10, 120, { 0, 180, 0, 255 });
    }

    // --- Сообщение "Сохранено!" ---
    if (lastSaveTick != 0 && SDL_GetTicks() - lastSaveTick < 2000) {
        SDL_Rect savedSize = getTextSize("Saved!"); // Исправлено здесь
        drawText("Saved!", winW - savedSize.w - 20, 20, { 0, 200, 0, 255 });
    }

    // --- Инспектор узла ---
    if (!inspectorNodeId.empty()) {
        const Node* n = graphManager.getNode(inspectorNodeId);
        if (n) {
            int y_offset = 200;
            auto drawInfoLine = [&](const std::string& text) {
                drawText(text, 20, y_offset, { 0,0,0,255 });
                y_offset += 20;
                };
            drawInfoLine("== Inspector ==");
            drawInfoLine("ID: " + n->id);
            drawInfoLine("Building: " + n->building);
            drawInfoLine("Floor: " + std::to_string(n->floor));
            drawInfoLine("Coords: (" + std::to_string(n->x) + "," + std::to_string(n->y) + ")");
            drawInfoLine(std::string("Portal: ") + (n->isPortal ? "true" : "false"));
            drawInfoLine("Neighbors (" + std::to_string(n->neighbors.size()) + "):");
            for (const auto& nb : n->neighbors) {
                drawInfoLine(" - " + nb);
            }
        }
    }
#endif
}

void MapRenderer::drawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font.get(), text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst{ x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if (!tex) return;
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

SDL_Rect MapRenderer::getTextSize(const std::string& text) {
    if (!font || text.empty()) return { 0,0,0,0 };
    int w, h;
    TTF_SizeText(font.get(), text.c_str(), &w, &h);
    return { 0, 0, w, h };
}