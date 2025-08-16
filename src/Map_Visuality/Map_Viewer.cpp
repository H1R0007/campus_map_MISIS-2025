#include "Map_Viewer.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


MapViewer::MapViewer(SDL_Renderer* renderer, const char* mapPath)
    : renderer(renderer), mapSize{ Config::MAP_WIDTH, Config::MAP_HEIGHT }
{
    graph.loadFromJson("assets/nodes.json");
    loadMap(mapPath);

    // Центрируем карту внутри CANVAS
    mapRect.x = (Config::CANVAS_WIDTH - mapSize.x) / 2;
    mapRect.y = (Config::CANVAS_HEIGHT - mapSize.y) / 2;
    mapRect.w = mapSize.x;
    mapRect.h = mapSize.y;

    camera = Camera();  // Камера сама центрируется на Canvas в конструкторе
    font = TTF_OpenFont("assets/fonts/Roboto-Regular.ttf", 16);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
    }
}

void MapViewer::renderOverlay() {
    if (!font) return;

    int percent = static_cast<int>(camera.getScale() * 100);
    std::string scaleLabel = std::to_string(percent) + "%";

    std::string coordsLabel = "X=" + std::to_string(debugMouseWorld.x) +
        " Y=" + std::to_string(debugMouseWorld.y);

    SDL_Color textColor = { 0, 0, 0, 255 };

    // рисуем масштаб
    SDL_Surface* surf1 = TTF_RenderText_Blended(font, scaleLabel.c_str(), textColor);
    SDL_Texture* tex1 = SDL_CreateTextureFromSurface(renderer, surf1);
    SDL_Rect dst1{ 10, 10, surf1->w, surf1->h };
    SDL_FreeSurface(surf1);
    SDL_RenderCopy(renderer, tex1, nullptr, &dst1);
    SDL_DestroyTexture(tex1);

    // рисуем координаты курсора
    SDL_Surface* surf2 = TTF_RenderText_Blended(font, coordsLabel.c_str(), textColor);
    SDL_Texture* tex2 = SDL_CreateTextureFromSurface(renderer, surf2);
    SDL_Rect dst2{ 10, 30, surf2->w, surf2->h }; // чуть ниже масштаба
    SDL_FreeSurface(surf2);
    SDL_RenderCopy(renderer, tex2, nullptr, &dst2);
    SDL_DestroyTexture(tex2);

    if (hoveredNode) {
        std::string nodeLabel = "Hovered: " + hoveredNode->id;
        SDL_Color nodeColor = { 128, 0, 128, 255 }; // фиолетовый

        SDL_Surface* surf = TTF_RenderText_Blended(font, nodeLabel.c_str(), nodeColor);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst{ 10, 50, surf->w, surf->h }; // ниже координат
        SDL_FreeSurface(surf);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
}

void MapViewer::loadMap(const char* path) {
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        std::cerr << "Failed to load map: " << IMG_GetError() << std::endl;
        return;
    }
    mapTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
}

void MapViewer::handleEvent(SDL_Event& event) {
    if (event.type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&lastMousePos.x, &lastMousePos.y);
        float zoomFactor = (event.wheel.y > 0) ? 1.1f : 0.9f;

        // Проверяем Ctrl
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) {
            camera.zoom(zoomFactor, ZoomMode::Mouse, lastMousePos);
        }
        else {
            camera.zoom(zoomFactor, ZoomMode::Center);
        }
    }

    if (event.type == SDL_MOUSEMOTION) {
        // конвертация в мировой XY
        SDL_Point screenPos{ event.motion.x, event.motion.y };
        debugMouseWorld = camera.screenToWorld(screenPos);

        // если зажата ЛКМ — перемещаем карту
        if (event.motion.state & SDL_BUTTON_LMASK) {
            camera.move(-event.motion.xrel, -event.motion.yrel);
        }

        if (Config::DEV_MODE) {
            hoveredNode = nullptr;
            for (auto& [id, node] : graph.getNodes()) {
                SDL_Point world{ node.x, node.y };
                SDL_Point scr = camera.worldToScreen(world);

                int dx = scr.x - event.motion.x;
                int dy = scr.y - event.motion.y;
                if (dx * dx + dy * dy <= 25) { // радиус ~5px
                    hoveredNode = &node;
                    break;
                }
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_Point clickScreen{ event.button.x, event.button.y };
            SDL_Point clickWorld = camera.screenToWorld(clickScreen);

            if (Config::DEV_MODE) {
                const Uint8* keys = SDL_GetKeyboardState(nullptr);

                if (keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT]) {
                    // выбрали активный узел
                    for (auto& [id, node] : graph.getNodesMutable()) {
                        SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                        int dx = scr.x - clickScreen.x;
                        int dy = scr.y - clickScreen.y;
                        if (dx * dx + dy * dy <= 25) {
                            activeNodeId = id;
                            neighborMode = true;
                            std::cout << "Neighbor mode started for " << id << "\n";
                            break;
                        }
                    }
                }
                else if (neighborMode && !activeNodeId.empty()) {
                    // добавляем соседа
                    for (auto& [id, node] : graph.getNodesMutable()) {
                        SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                        int dx = scr.x - clickScreen.x;
                        int dy = scr.y - clickScreen.y;
                        if (dx * dx + dy * dy <= 25 && id != activeNodeId) {
                            Node& activeNode = graph.getNodesMutable()[activeNodeId];

                            // двухсторонняя связь
                            if (std::find(activeNode.neighbors.begin(), activeNode.neighbors.end(), id) == activeNode.neighbors.end()) {
                                activeNode.neighbors.push_back(id);
                            }
                            if (std::find(node.neighbors.begin(), node.neighbors.end(), activeNode.id) == node.neighbors.end()) {
                                node.neighbors.push_back(activeNode.id);
                            }

                            graph.saveToJson("assets/nodes.json");
                            std::cout << "Added " << id << " as neighbor to " << activeNode.id << "\n";
                            break;
                        }
                    }
                }
                else {
                    // создание нового узла
                    graph.addNode(clickWorld.x, clickWorld.y);
                }
            }
        }
    }
    if (event.type == SDL_KEYDOWN) {

        if (event.key.keysym.sym == SDLK_RETURN ||
            event.key.keysym.sym == SDLK_KP_ENTER) {
            std::cout << "RETURN pressed\n";
            if (neighborMode && !activeNodeId.empty()) {
                std::cout << "Neighbor mode ended for " << activeNodeId << "\n";
            }
            neighborMode = false;
            activeNodeId.clear();
        }

        if (event.key.keysym.sym == SDLK_ESCAPE) {
            std::cout << "ESC pressed\n";
            neighborMode = false;
            activeNodeId.clear();
        }

        if ((event.key.keysym.sym == SDLK_z) && (SDL_GetModState() & KMOD_CTRL)) {
            std::cout << "CTRL+Z pressed\n";
            if (Config::DEV_MODE) {
                graph.removeLastNode();
            }
        }
    }
}

void MapViewer::render() {
    // viewport камеры в мировых координатах (Canvas)
    SDL_Rect viewportWorld = camera.getViewport();

    // карта в Canvas (не меняется)
    SDL_Rect mapInWorld = {
        static_cast<int>(mapRect.x * camera.getScale()),
        static_cast<int>(mapRect.y * camera.getScale()),
        static_cast<int>(mapRect.w * camera.getScale()),
        static_cast<int>(mapRect.h * camera.getScale())
    };

    // пересечение viewport и карты
    SDL_Rect intersection;
    if (SDL_IntersectRect(&viewportWorld, &mapInWorld, &intersection)) {
        SDL_Rect srcRect;
        SDL_Rect destRect;

        // Откуда берём пиксели на текстуре
        srcRect.x = (intersection.x - mapInWorld.x) / camera.getScale();
        srcRect.y = (intersection.y - mapInWorld.y) / camera.getScale();
        srcRect.w = intersection.w / camera.getScale();
        srcRect.h = intersection.h / camera.getScale();

        // Куда рисуем на экране
        destRect.x = intersection.x - viewportWorld.x;
        destRect.y = intersection.y - viewportWorld.y;
        destRect.w = intersection.w;
        destRect.h = intersection.h;

        SDL_RenderCopy(renderer, mapTexture, &srcRect, &destRect);
    }
    // Что вне карты — останется фоном окна
    // 
    // Отрисуем связи (рёбра графа)
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255); // серые линии

    for (auto& [id, node] : graph.getNodes()) {
        SDL_Point src = camera.worldToScreen({ node.x, node.y });
        for (auto& nbId : node.neighbors) {
            auto nbPtr = graph.getNode(nbId);
            if (nbPtr) {
                SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });

                // Чтобы не рисовать линии дважды, берём только id < nbId
                if (id < nbId) {
                    if (neighborMode && !activeNodeId.empty() && (id == activeNodeId || nbId == activeNodeId)) {
                        SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255);  // зелёный для активного
                    }
                    else {
                        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255); // серый
                    }
                    SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                }
            }
        }
    }

    // Отрисуем узлы поверх карты
    if (debugDrawNodes) {
        for (auto& [id, node] : graph.getNodes()) {
            SDL_Point scr = camera.worldToScreen({ node.x, node.y });
            SDL_Rect rect{ scr.x - 3, scr.y - 3, 6, 6 };

            if (neighborMode && !activeNodeId.empty() && id == activeNodeId) {
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // зелёный
            }
            else {
                SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // красный
            }
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

MapViewer::~MapViewer() {
    if (mapTexture) SDL_DestroyTexture(mapTexture);
    if (font) TTF_CloseFont(font);
}