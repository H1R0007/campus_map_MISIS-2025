#include "Map_Viewer.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

// === Lifecycle ===
MapViewer::MapViewer(SDL_Renderer* renderer, const char* /*mapPath*/)
    : renderer(renderer), mapSize{ Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT }
{
    // Загружаем граф кампуса + метаданные + переходы
    graphManager.loadCampus(Config::CAMPUS_GRAPH_PATH);
    graphManager.loadCampusMeta(Config::CAMPUS_META_PATH);
    graphManager.loadTransitions(Config::TRANSITIONS_PATH);

    // Начинаем с кампуса
    currentView = ViewMode::Campus;
    currentBuilding.clear();
    currentFloor = 0;
    graphManager.setActiveGraph("__campus");

    // Начальная карта → campus map
    mapTexture = nullptr;
    loadMap(Config::CAMPUS_MAP_PATH);

    // Алиасы
    aliasManager.load(Config::ALIASES_PATH);

    // Центрируем карту в Canvas
    mapRect.x = (Config::CANVAS_WIDTH - mapSize.x) / 2;
    mapRect.y = (Config::CANVAS_HEIGHT - mapSize.y) / 2;
    mapRect.w = mapSize.x;
    mapRect.h = mapSize.y;

    camera = Camera();
    camera.setWorldSize(Config::CANVAS_WIDTH, Config::CANVAS_HEIGHT);

    // Шрифт
    font = TTF_OpenFont(Config::FONT_PATH, 16);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
    }
}
MapViewer::~MapViewer() {
    if (mapTexture) SDL_DestroyTexture(mapTexture);
    if (font) TTF_CloseFont(font);
}
void MapViewer::onWindowResized(int w, int h) {
    camera.setViewportSize(w, h);
}

// === Rendering ===
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

    const auto& nodesToRender =
        (currentView == ViewMode::Campus)
        ? graphManager.getCampusNodes()
        : graphManager.getActiveNodes();

    // ======== Всё ниже — только для DEV_MODE ========
    if (Config::DEV_MODE) {
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);

        // Размер окна
        int winW, winH;
        SDL_GetRendererOutputSize(renderer, &winW, &winH);

        // Видимые мировые границы
        SDL_Point topLeftWorld = camera.screenToWorld({ 0, 0 });
        SDL_Point botRightWorld = camera.screenToWorld({ winW, winH });

        int baseStep = 100;
        int step = baseStep;

        // Адаптивность: минимум 25px в масштабе
        while (step * camera.getScale() < 25) {
            step *= 2;
        }

        // Нормализуем стартовые координаты
        int startX = (topLeftWorld.x / step) * step;
        int startY = (topLeftWorld.y / step) * step;

        // Вертикальные линии
        for (int x = startX; x <= botRightWorld.x; x += step) {
            SDL_Point scrA = camera.worldToScreen({ x, topLeftWorld.y });
            SDL_Point scrB = camera.worldToScreen({ x, botRightWorld.y });
            SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
        }

        // Горизонтальные линии
        for (int y = startY; y <= botRightWorld.y; y += step) {
            SDL_Point scrA = camera.worldToScreen({ topLeftWorld.x, y });
            SDL_Point scrB = camera.worldToScreen({ botRightWorld.x, y });
            SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
        }
    }

    if (Config::DEV_MODE) {
        // 1) Постоянные рёбра (серые)
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        for (auto& [id, node] : nodesToRender) {
            SDL_Point src = camera.worldToScreen({ node.x, node.y });
            for (auto& nbId : node.neighbors) {
                const Node* nbPtr = graphManager.getNode(nbId);
                if (nbPtr && id < nbId) {
                    SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                    SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                }
            }
        }

        // 2) Временные рёбра (жёлтые)
        if (neighborMode && !activeNodeId.empty()) {
            auto activeNode = graphManager.getNode(activeNodeId);
            if (activeNode) {
                SDL_Point src = camera.worldToScreen({ activeNode->x, activeNode->y });
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
                for (auto& nbId : pendingNeighbors) {
                    auto nbPtr = graphManager.getNode(nbId);
                    if (nbPtr) {
                        SDL_Point dst = camera.worldToScreen({ nbPtr->x, nbPtr->y });
                        SDL_RenderDrawLine(renderer, src.x, src.y, dst.x, dst.y);
                    }
                }
            }
        }

        // ПОРТАЛ (линия от выбранного первого узла)
        if (!portalStartNode.empty()) {
            const Node* p = graphManager.getNode(portalStartNode);
            if (p) {
                SDL_Point scr = camera.worldToScreen({ p->x, p->y });
                SDL_SetRenderDrawColor(renderer, 0, 200, 200, 255);
                SDL_Rect border{ scr.x - 6, scr.y - 6, 12, 12 };
                SDL_RenderDrawRect(renderer, &border);
            }
        }

        // 3) Узлы (зелёный активный, красные остальные, hover — синий контур)
        if (debugDrawNodes) {
            for (auto& [id, node] : nodesToRender) {
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

    // ====== Отрисовка найденного пути ======
    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

        for (size_t i = 1; i < currentPath.size(); i++) {
            const Node* from = graphManager.getNode(currentPath[i - 1]);
            const Node* to = graphManager.getNode(currentPath[i]);
            if (!from || !to) continue;

            bool shouldDraw = false;

            // Правило фильтрации
            if (currentView == ViewMode::Campus) {
                // оба узла должны принадлежать кампусу
                if (currentView == ViewMode::Campus) {
                    if (from->building == "CAMPUS" && to->building == "CAMPUS")
                        shouldDraw = true;
                }
            }
            else if (currentView == ViewMode::BuildingFloor) {
                // оба узла должны принадлежать текущему корпусу и этажу
                if (from->building == currentBuilding && to->building == currentBuilding &&
                    from->floor == currentFloor && to->floor == currentFloor)
                    shouldDraw = true;
            }

            if (shouldDraw) {
                SDL_Point scrA = camera.worldToScreen({ from->x, from->y });
                SDL_Point scrB = camera.worldToScreen({ to->x, to->y });
                SDL_RenderDrawLine(renderer, scrA.x, scrA.y, scrB.x, scrB.y);
            }
        }
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


    // Панель поиска маршрута
    SDL_Color black = { 0,0,0,255 };

    // --- Field "OTKYDA" ---
    std::string labelFrom = "OTKYDA: " + inputFrom + (editingFrom ? "_" : "");
    SDL_Surface* surfFrom = TTF_RenderText_Blended(font, labelFrom.c_str(), black);
    SDL_Texture* texFrom = SDL_CreateTextureFromSurface(renderer, surfFrom);
    SDL_Rect dstFrom{ 10, 60, surfFrom->w, surfFrom->h };
    SDL_FreeSurface(surfFrom);
    SDL_RenderCopy(renderer, texFrom, nullptr, &dstFrom);
    SDL_DestroyTexture(texFrom);

    fromFieldRect = dstFrom;

    // --- Field "KYDA" ---
    std::string labelTo = "KYDA: " + inputTo + (!editingFrom ? "_" : "");
    SDL_Surface* surfTo = TTF_RenderText_Blended(font, labelTo.c_str(), black);
    SDL_Texture* texTo = SDL_CreateTextureFromSurface(renderer, surfTo);
    SDL_Rect dstTo{ 10, 90, surfTo->w, surfTo->h };
    SDL_FreeSurface(surfTo);
    SDL_RenderCopy(renderer, texTo, nullptr, &dstTo);
    SDL_DestroyTexture(texTo);

    toFieldRect = dstTo;

    // === USER OPTIONS ===
    std::string optsText = "Options: ";
    optsText += "Stairs=";  optsText += (userAllowStairs ? "ON" : "OFF");
    optsText += " | Lift="; optsText += (userAllowLift ? "ON" : "OFF");
    optsText += " | Bridge="; optsText += (userAllowBridge ? "ON" : "OFF");

    SDL_Color optColor = { 50, 50, 50, 255 };
    SDL_Surface* surfOpts = TTF_RenderText_Blended(font, optsText.c_str(), optColor);
    SDL_Texture* texOpts = SDL_CreateTextureFromSurface(renderer, surfOpts);

    int winW, winH;
    SDL_GetRendererOutputSize(renderer, &winW, &winH);

    // позиция внизу справа
    SDL_Rect dstOpts{ winW - surfOpts->w - 20, winH - surfOpts->h - 20, surfOpts->w, surfOpts->h };
    SDL_FreeSurface(surfOpts);
    SDL_RenderCopy(renderer, texOpts, nullptr, &dstOpts);
    SDL_DestroyTexture(texOpts);

    // Suggestions rendered at top-right corner
    std::string currentInput = editingFrom ? inputFrom : inputTo;

    // показываем подсказки только если пользователь что-то ввёл
    currentSuggestions.clear();
    if (!currentInput.empty()) {
        currentSuggestions = aliasManager.suggest(currentInput, 3);
    }

    int winW1, winH1;
    SDL_GetRendererOutputSize(renderer, &winW1, &winH1);

    int baseX = winW1 - 250; // отступ справа
    int baseY = 50;         // отступ сверху
    int offset = 0;

    for (int i = 0; i < (int)currentSuggestions.size(); i++) {
        SDL_Color color = (i == selectedSuggestionIndex)
            ? SDL_Color{ 200,0,0,255 }   // выделение красным
        : SDL_Color{ 0,0,200,255 }; // обычные варианты – синим

        SDL_Surface* surf = TTF_RenderText_Blended(font, currentSuggestions[i].c_str(), color);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst{ baseX, baseY + offset, surf->w, surf->h };
        SDL_FreeSurface(surf);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);

        offset += 20;
    }

    if (Config::DEV_MODE) {

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

    // === Dev Node Inspector ===
    if (Config::DEV_MODE && !inspectorNodeId.empty()) {
        const Node* n = graphManager.getNode(inspectorNodeId);
        if (n && font) {
            int x0 = 20;
            int y0 = 200; // позиция панели
            SDL_Color black = { 0,0,0,255 };

            auto drawLine = [&](const std::string& text, int dy) {
                SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), black);
                if (!surf) {
                    std::cout << "TTF_RenderText_Blended failed: " << TTF_GetError() << "\n";
                    return 0;
                }
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (!tex) {
                    std::cout << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << "\n";
                    SDL_FreeSurface(surf);
                    return 0;
                }
                SDL_Rect dst{ x0, y0 + dy, surf->w, surf->h };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
                int h = surf->h;
                SDL_FreeSurface(surf);
                return h + 4;
                };

            int offset = 0;
            offset += drawLine("Inspector:", offset);
            offset += drawLine("ID: " + n->id, offset);
            offset += drawLine("Building: " + n->building, offset);
            offset += drawLine("Floor: " + std::to_string(n->floor), offset);

            std::string coords = "Coords: (" + std::to_string(n->x)
                + "," + std::to_string(n->y) + ")";
            offset += drawLine(coords, offset);

            offset += drawLine(std::string("Portal: ") + (n->isPortal ? "true" : "false"), offset);

            offset += drawLine("Neighbors:", offset);
            for (auto& nb : n->neighbors) {
                offset += drawLine(" - " + nb, offset);
            }
        }
        else {
            std::cout << "[Inspector] node removed: " << inspectorNodeId << " -> closing\n";
            inspectorNodeId.clear();
        }
    }
}

// === Events ===
void MapViewer::handleEvent(SDL_Event& event) {

    // === Mouse Button Events ===
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        SDL_Point clickScreen{ event.button.x, event.button.y };
        SDL_Point clickWorld = camera.screenToWorld(clickScreen);

        // --- Input focus switching (left click on fields) ---
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (SDL_PointInRect(&clickScreen, &fromFieldRect)) {
                inputActive = true;
                editingFrom = true;
                std::cout << "[UI] Focused FROM field\n";
                return; // block further event handling
            }
            if (SDL_PointInRect(&clickScreen, &toFieldRect)) {
                inputActive = true;
                editingFrom = false;
                std::cout << "[UI] Focused TO field\n";
                return;
            }
            // click elsewhere → clear focus
            if (inputActive) {
                inputActive = false;
                std::cout << "[UI] Input focus cleared\n";
            }
        }

        // --- LEFT CLICK ---
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (Config::DEV_MODE && !neighborMode) {
                std::string clickedId = findNodeUnderCursor(clickScreen);
                if (!clickedId.empty()) {
                    if (startNodeId.empty()) {
                        startNodeId = clickedId;
                        std::cout << "Start selected: " << startNodeId << "\n";
                    }
                    else if (endNodeId.empty()) {
                        endNodeId = clickedId;
                        std::cout << "End selected: " << endNodeId << "\n";
                        currentPath = find_shortest_path(startNodeId, endNodeId, graphManager);
                        if (currentPath.empty()) {
                            std::cout << "Path not found!\n";
                        }
                        else {
                            std::cout << "Path has: " << currentPath.size() << " steps\n";
                        }
                    }
                    else {
                        startNodeId.clear();
                        endNodeId.clear();
                        currentPath.clear();
                        std::cout << "Reset path selection\n";
                    }
                }
            }
        }

        // --- RIGHT CLICK ---
        if (event.button.button == SDL_BUTTON_RIGHT) {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);

            // [Ctrl+RightClick] → Inspector toggle
            if (Config::DEV_MODE && keys[SDL_SCANCODE_LCTRL]) {
                std::string target = findNodeUnderCursor(clickScreen);
                if (!target.empty()) {
                    if (inspectorNodeId == target) {
                        inspectorNodeId.clear();
                        std::cout << "[Inspector] Closed for node " << target << "\n";
                    }
                    else {
                        inspectorNodeId = target;
                        std::cout << "[Inspector] Selected node " << inspectorNodeId << "\n";
                    }
                }
                return;
            }

            // [Shift+RightClick] → delete node or edge
            if (Config::DEV_MODE && (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])) {
                std::string target = findNodeUnderCursor(clickScreen);
                if (!target.empty()) {
                    if (neighborMode && !activeNodeId.empty() && activeNodeId != target) {
                        auto it = std::find(pendingNeighbors.begin(), pendingNeighbors.end(), target);
                        if (it != pendingNeighbors.end()) {
                            pendingNeighbors.erase(it);
                            std::cout << "Removed staged neighbor " << target
                                << " from " << activeNodeId << "\n";
                        }
                        else {
                            graphManager.removeNeighbor(activeNodeId, target);
                            std::cout << "Removed edge between " << activeNodeId
                                << " and " << target << "\n";
                        }
                    }
                    else {
                        std::cout << "Removing node " << target << "\n";
                        graphManager.removeNodeById(target);
                    }
                }
                return;
            }

            // [Alt+RightClick] → neighbor mode / portal completion
            if (Config::DEV_MODE && (keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT])) {
                std::string target = findNodeUnderCursor(clickScreen);
                if (!target.empty()) {
                    if (!portalStartNode.empty() && portalStartNode != target) {
                        Transition tr{ portalStartNode, target, TransitionType::Door };
                        graphManager.addTransition(tr);
                        std::cout << "Portal created: " << portalStartNode
                            << " <-> " << target << "\n";
                        portalStartNode.clear();
                    }
                    else {
                        activeNodeId = target;
                        neighborMode = true;
                        pendingNeighbors.clear();
                        std::cout << "Neighbor mode started for " << target << "\n";
                    }
                }
                return;
            }

            // [P+RightClick] → portal editing
            if (Config::DEV_MODE && keys[SDL_SCANCODE_P]) {
                std::string target = findNodeUnderCursor(clickScreen);
                if (!target.empty()) {
                    if (portalStartNode.empty()) {
                        portalStartNode = target;
                        std::cout << "Portal start: " << portalStartNode << "\n";
                    }
                    else {
                        Transition tr{ portalStartNode, target, TransitionType::Door };
                        graphManager.addTransition(tr);
                        std::cout << "Portal created: " << portalStartNode
                            << " <-> " << target << "\n";
                        portalStartNode.clear();
                    }
                }
                return;
            }

            // [NeighborMode active + RightClick] → add staged neighbor
            if (Config::DEV_MODE && neighborMode && !activeNodeId.empty()) {
                std::string target = findNodeUnderCursor(clickScreen, 10); // bigger pick radius
                if (!target.empty() && target != activeNodeId &&
                    std::find(pendingNeighbors.begin(), pendingNeighbors.end(), target) == pendingNeighbors.end()) {
                    pendingNeighbors.push_back(target);
                    std::cout << "Staged neighbor " << target << " for " << activeNodeId << "\n";
                }
                return;
            }

            // [Default RightClick] → create new node
            if (Config::DEV_MODE && !neighborMode) {
                graphManager.addNode(clickWorld.x, clickWorld.y);
            }
        }
    }


    // === Mouse Motion ===
    if (event.type == SDL_MOUSEMOTION) {
        SDL_Point screenPos{ event.motion.x, event.motion.y };
        debugMouseWorld = camera.screenToWorld(screenPos);

        // Dragging map
        if (event.motion.state & SDL_BUTTON_LMASK) {
            camera.move(-event.motion.xrel, -event.motion.yrel);
        }

        // Hover highlight (DEV only)
        if (Config::DEV_MODE) {
            std::string target = findNodeUnderCursor(screenPos);
            hoveredNode = target.empty() ? nullptr : graphManager.getNode(target);
        }
    }


    // === Mouse Wheel ===
    if (event.type == SDL_MOUSEWHEEL) {
        SDL_GetMouseState(&lastMousePos.x, &lastMousePos.y);
        float zoomFactor = (event.wheel.y > 0) ? 1.1f : 0.9f;

        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) {
            camera.zoom(zoomFactor, ZoomMode::Mouse, lastMousePos);
        }
        else {
            camera.zoom(zoomFactor, ZoomMode::Center);
        }
    }


    // === Keyboard (developer functions) ===
    if (event.type == SDL_KEYDOWN && Config::DEV_MODE) {
        switch (event.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (neighborMode && !activeNodeId.empty()) {
                for (auto& nb : pendingNeighbors) {
                    graphManager.addNeighbor(activeNodeId, nb);
                }
                std::cout << "Neighbor mode ended for "
                    << activeNodeId << ", saved "
                    << pendingNeighbors.size() << " neighbors\n";
                neighborMode = false;
                activeNodeId.clear();
                pendingNeighbors.clear();
            }
            break;
        case SDLK_TAB:
            if (currentView == ViewMode::Campus) {
                if (!currentBuilding.empty())
                    switchToFloor(currentBuilding, currentFloor);
            }
            else {
                currentView = ViewMode::Campus;
                graphManager.setActiveGraph("__campus");
                loadMap(Config::CAMPUS_MAP_PATH);
                std::cout << "Switched to CAMPUS\n";
            }
            break;
        case SDLK_RIGHT:
            if (currentBuilding == "Building_A") switchToFloor("Building_B", 1);
            else if (currentBuilding == "Building_B") switchToFloor("Building_C", 1);
            else switchToFloor("Building_A", 1);
            break;
        case SDLK_LEFT:
            if (currentBuilding == "Building_C") switchToFloor("Building_B", 1);
            else if (currentBuilding == "Building_B") switchToFloor("Building_A", 1);
            else switchToFloor("Building_C", 1);
            break;
        case SDLK_UP: {
            const BuildingMeta* bm = graphManager.getBuildingMeta(currentBuilding);
            if (bm) {
                for (int i = 0; i < (int)bm->floors.size(); i++) {
                    if (bm->floors[i].floor == currentFloor && i + 1 < (int)bm->floors.size()) {
                        switchToFloor(currentBuilding, bm->floors[i + 1].floor);
                        break;
                    }
                }
            }
            break;
        }
        case SDLK_DOWN: {
            const BuildingMeta* bm = graphManager.getBuildingMeta(currentBuilding);
            if (bm) {
                for (int i = 0; i < (int)bm->floors.size(); i++) {
                    if (bm->floors[i].floor == currentFloor && i > 0) {
                        switchToFloor(currentBuilding, bm->floors[i - 1].floor);
                        break;
                    }
                }
            }
            break;
        }
        case SDLK_ESCAPE:
            std::cout << "Neighbor mode cancelled for " << activeNodeId
                << ", discarded " << pendingNeighbors.size() << " staged neighbors\n";
            neighborMode = false;
            activeNodeId.clear();
            pendingNeighbors.clear();
            break;
        default:
            break;
        }

        // Ctrl+Z / Ctrl+Y Undo/Redo
        if ((event.key.keysym.sym == SDLK_z) && (SDL_GetModState() & KMOD_CTRL)) {
            std::cout << "CTRL+Z Undo\n";
            graphManager.undoGlobal();
        }
        if ((event.key.keysym.sym == SDLK_y) && (SDL_GetModState() & KMOD_CTRL)) {
            std::cout << "CTRL+Y Redo\n";
            graphManager.redoGlobal();
        }

        // Ctrl+S Save
        if ((event.key.keysym.sym == SDLK_s) && (SDL_GetModState() & KMOD_CTRL)) {
            if (SDL_GetModState() & KMOD_SHIFT) {
                std::cout << "CTRL+SHIFT+S Save transitions\n";
                graphManager.saveTransitions("assets/transitions/transitions.json");
            }
            else {
                std::cout << "CTRL+S Save graph\n";
                lastSaveTick = SDL_GetTicks();
                graphManager.saveActive();
            }
        }

        // Toggle user options
        if (event.key.keysym.sym == SDLK_8) {
            userAllowStairs = !userAllowStairs;
            std::cout << "Option: allowStairs = " << userAllowStairs << "\n";
        }
        if (event.key.keysym.sym == SDLK_9) {
            userAllowLift = !userAllowLift;
            std::cout << "Option: allowLift = " << userAllowLift << "\n";
        }
        if (event.key.keysym.sym == SDLK_0) {
            userAllowBridge = !userAllowBridge;
            std::cout << "Option: allowBridge = " << userAllowBridge << "\n";
        }
    }


    // === Text input only if field focused ===
    if (event.type == SDL_TEXTINPUT && inputActive) {
        if (editingFrom) inputFrom += event.text.text;
        else inputTo += event.text.text;
        selectedSuggestionIndex = -1;
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_TAB: // switch input field
            if (inputActive) {
                editingFrom = !editingFrom;
                selectedSuggestionIndex = -1;
            }
            break;
        case SDLK_BACKSPACE:
            if (editingFrom && !inputFrom.empty()) inputFrom.pop_back();
            if (!editingFrom && !inputTo.empty()) inputTo.pop_back();
            if ((editingFrom && inputFrom.empty()) || (!editingFrom && inputTo.empty())) {
                currentSuggestions.clear();
                selectedSuggestionIndex = -1;
            }
            else {
                selectedSuggestionIndex = -1;
            }
            break;
        case SDLK_DOWN:
            if (!currentSuggestions.empty()) {
                selectedSuggestionIndex++;
                if (selectedSuggestionIndex >= (int)currentSuggestions.size())
                    selectedSuggestionIndex = 0;
            }
            break;
        case SDLK_UP:
            if (!currentSuggestions.empty()) {
                selectedSuggestionIndex--;
                if (selectedSuggestionIndex < 0)
                    selectedSuggestionIndex = (int)currentSuggestions.size() - 1;
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (!currentSuggestions.empty() && selectedSuggestionIndex >= 0) {
                if (editingFrom) inputFrom = currentSuggestions[selectedSuggestionIndex];
                else inputTo = currentSuggestions[selectedSuggestionIndex];
                selectedSuggestionIndex = -1;
            }
            else if (!inputFrom.empty() && !inputTo.empty()) {
                buildPathFromAliases(inputFrom, inputTo);
            }
            break;
        default:
            break;
        }
    }
}

// === Pathfinding ===
void MapViewer::buildPathFromAliases(const std::string& startName, const std::string& endName) {
    std::string startId = aliasManager.resolve(startName);
    std::string endId = aliasManager.resolve(endName);

    if (startId.empty() || endId.empty()) {
        std::cout << "Alias not found: " << startName << " or " << endName << "\n";
        currentPath.clear();
        return;
    }

    PathFinderOptions opts;
    opts.allowStairs = userAllowStairs;
    opts.allowLift = userAllowLift;
    opts.allowBridge = userAllowBridge;

    currentPath = find_shortest_path(startId, endId, graphManager, opts);

    if (currentPath.empty()) {
        std::cout << "Путь не найден между " << startName << " и " << endName << "\n";
    }
    else {
        std::cout << "Путь построен: " << startName << " -> " << endName
            << " (" << currentPath.size() << " шагов)\n";
    }
}
void MapViewer::switchToFloor(const std::string& buildingId, int floor) {
    const BuildingMeta* bm = graphManager.getBuildingMeta(buildingId);
    if (!bm) return;

    for (auto& f : bm->floors) {
        if (f.floor == floor) {
            currentView = ViewMode::BuildingFloor;
            currentBuilding = buildingId;
            currentFloor = f.floor;

            graphManager.setActiveGraph(buildingId + "_floor_" + std::to_string(f.floor));
            loadMap(("assets/buildings/" + bm->id + "/" + f.mapPath).c_str());

            std::cout << "Switched to " << bm->name << " floor " << floor << "\n";
            return;
        }
    }
    std::cout << "Floor " << floor << " not found in building " << buildingId << "\n";
}

// === Private helpers ===
void MapViewer::loadMap(const char* path) {
    if (mapTexture) {
        SDL_DestroyTexture(mapTexture);
        mapTexture = nullptr;
    }

    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        std::cerr << "Failed to load map: " << path
            << " | SDL_image error: " << IMG_GetError() << std::endl;
        return;
    }

    mapTexture = SDL_CreateTextureFromSurface(renderer, surface);

    mapSize.x = surface->w;
    mapSize.y = surface->h;

    mapRect.w = surface->w;
    mapRect.h = surface->h;
    mapRect.x = (Config::CANVAS_WIDTH - mapRect.w) / 2;
    mapRect.y = (Config::CANVAS_HEIGHT - mapRect.h) / 2;

    SDL_FreeSurface(surface);

    if (!mapTexture) {
        std::cerr << "Failed to create texture " << SDL_GetError() << std::endl;
    }
}
