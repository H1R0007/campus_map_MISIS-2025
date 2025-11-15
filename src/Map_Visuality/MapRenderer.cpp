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
    const SDL_Point& lineToolEnd,
    const std::string& selectedFromId,
    const std::string& selectedToId)
{
    // 1. Отрисовка карты (как было)
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

#ifndef EMSCRIPTEN
    // 2. DEV-элементы (как было)
    if (devMode) {
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

        // Рёбра
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        for (const auto& [id, node] : nodesToRender) {
            SDL_Point src = camera.worldToScreen({ node.x, node.y });
            for (const auto& nbId : node.neighbors) {
                const Node* nbPtr = graphManager.getNode(nbId);
                if (nbPtr && id < nbId) {
                    SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                    SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                }
            }
        }

        // pendingNeighbors (как было)
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

        // portalStartNode (как было)
        if (!portalStartNode.empty()) {
            const Node* p = graphManager.getNode(portalStartNode);
            if (p) {
                SDL_Point scr = camera.worldToScreen({ p->x, p->y });
                SDL_SetRenderDrawColor(renderer, 0, 200, 200, 255);
                SDL_Rect border{ scr.x - 6, scr.y - 6, 12, 12 };
                SDL_RenderDrawRect(renderer, &border);
            }
        }

        // Узлы (как было)
        if (debugDrawNodes) {
            for (const auto& [id, node] : nodesToRender) {
                SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                SDL_Rect rect{ scr.x - 3, scr.y - 3, 6, 6 };
                if (neighborMode && !activeNodeId.empty() && id == activeNodeId) {
                    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
                }
                else {
                    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
                }
                SDL_RenderFillRect(renderer, &rect);

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

    // 3. Путь (как было, через ImGui drawlist)
    if (!currentPath.empty()) {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        ImU32 color = IM_COL32(55, 235, 255, 255);
        float thickness = 5.0f;
        float radius = thickness * 0.5f;

        for (size_t i = 1; i < currentPath.size(); ++i) {
            const Node* from = graphManager.getNode(currentPath[i - 1]);
            const Node* to = graphManager.getNode(currentPath[i]);
            if (!from || !to) continue;

            bool shouldDraw = false;
            if (currentView == "campus") {
                if (from->building == "CAMPUS" && to->building == "CAMPUS")
                    shouldDraw = true;
            }
            else {
                if (from->building == currentBuilding &&
                    to->building == currentBuilding &&
                    from->floor == currentFloor)
                    shouldDraw = true;
            }

            if (shouldDraw) {
                SDL_Point scrA = camera.worldToScreen({ from->x, from->y });
                SDL_Point scrB = camera.worldToScreen({ to->x, to->y });

                ImVec2 a(scrA.x, scrA.y);
                ImVec2 b(scrB.x, scrB.y);

                drawList->AddLine(a, b, color, thickness);
                drawList->AddCircleFilled(a, radius, color);
                drawList->AddCircleFilled(b, radius, color);
            }
        }
    }

    // 4. Маркеры начала/конца маршрута (USER mode)
    if (!devMode) {
        auto shouldDrawNodeHere = [&](const Node* n) -> bool {
            if (!n) return false;
            if (currentView == "campus") {
                return n->building == "CAMPUS";
            }
            else {
                return (n->building == currentBuilding && n->floor == currentFloor);
            }
            };

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        auto drawMarker = [&](const Node* n, ImU32 colFill, ImU32 colOutline) {
            if (!n) return;
            SDL_Point p = camera.worldToScreen({ n->x, n->y });
            ImVec2 c((float)p.x, (float)p.y);
            float r = 10.0f; // можно масштабировать от зума при желании
            drawList->AddCircleFilled(c, r, colFill, 32);
            drawList->AddCircle(c, r, colOutline, 32, 2.0f);
            drawList->AddCircleFilled(c, 3.0f, IM_COL32(255, 255, 255, 230), 16);
            };

        const Node* from = selectedFromId.empty() ? nullptr : graphManager.getNode(selectedFromId);
        const Node* to = selectedToId.empty() ? nullptr : graphManager.getNode(selectedToId);

        if (from && shouldDrawNodeHere(from)) {
            drawMarker(from, IM_COL32(30, 180, 90, 200), IM_COL32(15, 120, 60, 255));
        }
        if (to && shouldDrawNodeHere(to)) {
            drawMarker(to, IM_COL32(230, 60, 70, 200), IM_COL32(190, 30, 40, 255));
        }
    }
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