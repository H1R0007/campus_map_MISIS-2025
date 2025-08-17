#include "Map_Viewer.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


MapViewer::MapViewer(SDL_Renderer* renderer, const char* mapPath)
    : renderer(renderer), mapSize{ Config::MAP_WIDTH, Config::MAP_HEIGHT }
{
    graph.loadFromJson(Config::NODES_PATH);
    loadMap(mapPath);

    // Центрируем карту внутри CANVAS
    mapRect.x = (Config::CANVAS_WIDTH - mapSize.x) / 2;
    mapRect.y = (Config::CANVAS_HEIGHT - mapSize.y) / 2;
    mapRect.w = mapSize.x;
    mapRect.h = mapSize.y;

    camera = Camera();  
    camera.setWorldSize(mapRect.w, mapRect.h);
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

    if (Config::DEV_MODE) {
        std::string help = "Alt + RMB = choose node | RMB = add node / stage neighbor | Enter = confirm | Esc = cancel | Ctrl + Z = undo | Ctrl + Y = redo | Ctrl + S = save";
        SDL_Color gray = { 80, 80, 80, 255 };
        SDL_Surface* surf = TTF_RenderText_Blended(font, help.c_str(), gray);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst{ 10, Config::WINDOW_HEIGHT - surf->h - 10, surf->w, surf->h };
        SDL_FreeSurface(surf);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);

        // ---- сообщение о режиме добавления соседей ----
        if (neighborMode && !activeNodeId.empty()) {
            std::string editing = "Adding neighbors for: " + activeNodeId;
            SDL_Color green = { 0, 180, 0, 255 };
            SDL_Surface* surfEdit = TTF_RenderText_Blended(font, editing.c_str(), green);
            SDL_Texture* texEdit = SDL_CreateTextureFromSurface(renderer, surfEdit);
            SDL_Rect dstEdit{ 10, 90, surfEdit->w, surfEdit->h }; // ниже hovered
            SDL_FreeSurface(surfEdit);
            SDL_RenderCopy(renderer, texEdit, nullptr, &dstEdit);
            SDL_DestroyTexture(texEdit);
        }
    }
    if (Config::DEV_MODE && lastSaveTick != 0) {
        if (SDL_GetTicks() - lastSaveTick < 2000) { // 2 секунды
            SDL_Color green = { 0, 200, 0, 255 };
            SDL_Surface* surf = TTF_RenderText_Blended(font, "Saved!", green);
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst{ Config::WINDOW_WIDTH - surf->w - 20, 20, surf->w, surf->h };
            SDL_FreeSurface(surf);
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
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

    if (event.type == SDL_MOUSEBUTTONDOWN && Config::DEV_MODE) {
        if (event.button.button == SDL_BUTTON_RIGHT) {
            SDL_Point clickScreen{ event.button.x, event.button.y };
            SDL_Point clickWorld = camera.screenToWorld(clickScreen);

            const Uint8* keys = SDL_GetKeyboardState(nullptr);

            // ====== SHIFT + ПКМ = удаление узла или ребра ======
            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
                std::string targetNodeId;

                // Находим узел под курсором
                for (auto& [id, node] : graph.getNodes()) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25) {
                        targetNodeId = id;
                        break;
                    }
                }

                // Если нашли узел под курсором
                if (!targetNodeId.empty()) {
                    // Если есть активный узел (neighborMode) → удаляем ребро
                    if (neighborMode && !activeNodeId.empty() && activeNodeId != targetNodeId) {
                        std::cout << "Removing edge between "
                            << activeNodeId << " and " << targetNodeId << "\n";
                        graph.removeNeighbor(activeNodeId, targetNodeId);
                    }
                    else {
                        // Иначе удаляем сам узел
                        std::cout << "Removing node " << targetNodeId << "\n";
                        graph.removeNodeById(targetNodeId);
                    }
                    return;
                }
            }

            // ====== ALT + ПКМ = выбор activeNode ======
            if (keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT]) {
                for (auto& [id, node] : graph.getNodesMutable()) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25) {
                        activeNodeId = id;
                        neighborMode = true;
                        pendingNeighbors.clear();
                        std::cout << "Neighbor mode started for " << id << "\n";
                        break;
                    }
                }
            }

            else if (neighborMode && !activeNodeId.empty()) {
                // добавление соседа (staging)
                for (auto& [id, node] : graph.getNodesMutable()) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25 && id != activeNodeId) {
                        if (std::find(pendingNeighbors.begin(), pendingNeighbors.end(), id) == pendingNeighbors.end()) {
                            pendingNeighbors.push_back(id);
                            std::cout << "Staged neighbor "
                                << id << " for " << activeNodeId << "\n";
                        }
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

    if (event.type == SDL_KEYDOWN) {
        if (Config::DEV_MODE) {
            if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                if (neighborMode && !activeNodeId.empty()) {
                    for (auto& nb : pendingNeighbors) {
                        graph.addNeighbor(activeNodeId, nb);
                    }

                    std::cout << "Neighbor mode ended for "
                        << activeNodeId << ", saved "
                        << pendingNeighbors.size() << " neighbors\n";
                }

                neighborMode = false;
                activeNodeId.clear();
                pendingNeighbors.clear();
            }

            if (event.key.keysym.sym == SDLK_ESCAPE) {
                std::cout << "Neighbor mode cancelled for " << activeNodeId
                    << ", discarded " << pendingNeighbors.size() << " staged neighbors\n";
                neighborMode = false;
                activeNodeId.clear();
                pendingNeighbors.clear();
            }

            if ((event.key.keysym.sym == SDLK_z) && (SDL_GetModState() & KMOD_CTRL)) {
                std::cout << "CTRL+Z Undo\n";
                if (Config::DEV_MODE) graph.undo();
            }
            if ((event.key.keysym.sym == SDLK_y) && (SDL_GetModState() & KMOD_CTRL)) {
                std::cout << "CTRL+Y Redo\n";
                if (Config::DEV_MODE) graph.redo();
            }
            if ((event.key.keysym.sym == SDLK_s) && (SDL_GetModState() & KMOD_CTRL)) {
                if (Config::DEV_MODE) {
                    std::cout << "CTRL+S QuickSave\n";
                    lastSaveTick = SDL_GetTicks(); // запомнить время сохранения
                    graph.saveToJson("Config::NODES_PATH");
                }
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
        srcRect.w = std::max(1, int(intersection.w / camera.getScale()));
        srcRect.h = std::max(1, int(intersection.h / camera.getScale()));

        // Куда рисуем на экране
        destRect.x = intersection.x - viewportWorld.x;
        destRect.y = intersection.y - viewportWorld.y;
        destRect.w = intersection.w;
        destRect.h = intersection.h;

        SDL_RenderCopy(renderer, mapTexture, &srcRect, &destRect);
    }
    // ======== Всё ниже — только для DEV_MODE ========
    if (Config::DEV_MODE) {
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255); // светло-серый

        int step = 100; // шаг сетки
        int scaledCanvasW = static_cast<int>(Config::CANVAS_WIDTH * camera.getScale());
        int scaledCanvasH = static_cast<int>(Config::CANVAS_HEIGHT * camera.getScale());

        // Вертикальные линии
        for (int x = 0; x <= scaledCanvasW; x += step) {
            int screenX = x - camera.getViewport().x;
            SDL_RenderDrawLine(renderer, screenX, 0, screenX, Config::WINDOW_HEIGHT);
        }

        // Горизонтальные линии
        for (int y = 0; y <= scaledCanvasH; y += step) {
            int screenY = y - camera.getViewport().y;
            SDL_RenderDrawLine(renderer, 0, screenY, Config::WINDOW_WIDTH, screenY);
        }
    }

    if (Config::DEV_MODE) {
        // 1) Постоянные рёбра (серые)
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        for (auto& [id, node] : graph.getNodes()) {
            SDL_Point src = camera.worldToScreen({ node.x, node.y });
            for (auto& nbId : node.neighbors) {
                auto nbPtr = graph.getNode(nbId);
                if (nbPtr && id < nbId) {
                    SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                    SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                }
            }
        }

        // 2) Временные рёбра (жёлтые)
        if (neighborMode && !activeNodeId.empty()) {
            auto activeNode = graph.getNode(activeNodeId);
            if (activeNode) {
                SDL_Point src = camera.worldToScreen({ activeNode->x, activeNode->y });
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
                for (auto& nbId : pendingNeighbors) {
                    auto nbPtr = graph.getNode(nbId);
                    if (nbPtr) {
                        SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                        SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                    }
                }
            }
        }

        // 3) Узлы (зелёный активный, красные остальные, hover — синий контур)
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

                // hover подсветка (синий контур + подпись рядом)
                if (hoveredNode && hoveredNode->id == id) {
                    SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255);
                    SDL_Rect border{ scr.x - 5, scr.y - 5, 10, 10 };
                    SDL_RenderDrawRect(renderer, &border);

                    if (font) {
                        SDL_Color black = { 0, 0, 0, 255 };
                        SDL_Surface* surf = TTF_RenderText_Blended(font, id.c_str(), black);
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                        SDL_Rect dst{ scr.x + 10, scr.y - 10, surf->w, surf->h };
                        SDL_FreeSurface(surf);
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                }
            }
        }
    }
}


MapViewer::~MapViewer() {
    if (mapTexture) SDL_DestroyTexture(mapTexture);
    if (font) TTF_CloseFont(font);
}