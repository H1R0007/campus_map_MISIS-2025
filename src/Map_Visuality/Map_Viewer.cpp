#include "Map_Viewer.hpp"
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>

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

    // Поле "Откуда"
    std::string labelFrom = "OTKYDA: " + inputFrom + (editingFrom ? "_" : "");
    SDL_Surface* surfFrom = TTF_RenderText_Blended(font, labelFrom.c_str(), black);
    SDL_Texture* texFrom = SDL_CreateTextureFromSurface(renderer, surfFrom);
    SDL_Rect dstFrom{ 10, 60, surfFrom->w, surfFrom->h };
    SDL_FreeSurface(surfFrom);
    SDL_RenderCopy(renderer, texFrom, nullptr, &dstFrom);
    SDL_DestroyTexture(texFrom);

    // Поле "Куда"
    std::string labelTo = "KYDA: " + inputTo + (!editingFrom ? "_" : "");
    SDL_Surface* surfTo = TTF_RenderText_Blended(font, labelTo.c_str(), black);
    SDL_Texture* texTo = SDL_CreateTextureFromSurface(renderer, surfTo);
    SDL_Rect dstTo{ 10, 90, surfTo->w, surfTo->h };
    SDL_FreeSurface(surfTo);
    SDL_RenderCopy(renderer, texTo, nullptr, &dstTo);
    SDL_DestroyTexture(texTo);

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
    //Обработка для построения линий
    if (Config::DEV_MODE && lineMode) {
        std::string hint = lineStartSet ? "LineMode: click end point" : "LineMode: click start point";
        SDL_Color green{ 0, 200, 0, 255 };
        SDL_Surface* surf = TTF_RenderText_Blended(font, hint.c_str(), green);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst{ 10, 140, surf->w, surf->h }; // чуть ниже поиска
        SDL_FreeSurface(surf);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
}

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

void MapViewer::handleEvent(SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && Config::DEV_MODE) {
        if (Config::DEV_MODE && lineMode) {
            //Код для прямых линий
            SDL_Point clickWorld = camera.screenToWorld({ event.button.x, event.button.y });

            if (!lineStartSet) {
                lineStart = clickWorld;
                lineStartSet = true;
                std::cout << "Line start set at (" << lineStart.x << "," << lineStart.y << ")\n";
            }
            else {
                float dx = clickWorld.x - lineStart.x;
                float dy = clickWorld.y - lineStart.y;
                float angleDeg = atan2f(dy, dx) * 180.0f / M_PI;
                float length = std::sqrt(dx * dx + dy * dy);

                int spacing = std::max(Config::LINE_POINT_SPACING, Config::LINE_POINT_MIN_SPACING);

                // Считаем количество точек
                int count = (int)(length / spacing);

                // Ограничение сверху
                count = std::min(count, Config::LINE_POINT_MAX_COUNT);


                if (count > 0) {
                    graphManager.addLinearNodes(lineStart.x, lineStart.y, angleDeg, count, currentFloor);
                    std::cout << "Line finished → created " << count << " nodes\n";
                }
                else {
                    std::cout << "Line too short, no nodes created.\n";
                }

                lineMode = false;
                lineStartSet = false;
            }

            return;
        }
        if (Config::DEV_MODE) {
            SDL_Point clickScreen{ event.button.x, event.button.y };
            SDL_Point clickWorld = camera.screenToWorld(clickScreen);

            std::string clickedId;

            const auto& nodesHere =
                (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
                : graphManager.getActiveNodes();

            // Находим ближайший узел
            for (auto& [id, node] : nodesHere) {
                SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                int dx = scr.x - clickScreen.x;
                int dy = scr.y - clickScreen.y;
                if (dx * dx + dy * dy <= 25) {
                    clickedId = id;
                    break;
                }
            }

            if (!clickedId.empty()) {
                if (startNodeId.empty()) {
                    startNodeId = clickedId;
                    std::cout << "Start selected: " << startNodeId << "\n";
                }
                else if (endNodeId.empty()) {
                    endNodeId = clickedId;
                    std::cout << "End selected: " << endNodeId << "\n";

                    // Запуск алгоритма
                    currentPath = find_shortest_path(startNodeId, endNodeId, graphManager);

                    if (currentPath.empty()) {
                        std::cout << "Path not found!\n";
                    }
                    else {
                        std::cout << "Path has: " << currentPath.size() << " steps\n";
                    }
                }
                else {
                    // Сброс, если выбрали снова (третьим кликом)
                    startNodeId.clear();
                    endNodeId.clear();
                    currentPath.clear();
                    std::cout << "Sbros vibora tocheck\n";
                }
            }
        }
    }

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
            const auto& nodesHere =
                (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
                : graphManager.getActiveNodes();

            for (auto& [id, node] : nodesHere) {
                SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                int dx = scr.x - event.motion.x;
                int dy = scr.y - event.motion.y;
                if (dx * dx + dy * dy <= 25) {
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

            // ====== SHIFT + ПКМ ======
            // Удаление (узла или staged-соседа в neighborMode)
            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
                std::string target;
                const auto& nodesHere =
                    (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
                    : graphManager.getActiveNodes();
                for (auto& [id, node] : nodesHere) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25) { target = id; break; }
                }
                if (!target.empty()) {
                    // Если в режиме добавления соседей — сперва проверим staged list
                    if (neighborMode && !activeNodeId.empty() && activeNodeId != target) {
                        auto it = std::find(pendingNeighbors.begin(),
                            pendingNeighbors.end(), target);
                        if (it != pendingNeighbors.end()) {
                            pendingNeighbors.erase(it);
                            std::cout << "Removed staged neighbor "
                                << target << " from " << activeNodeId << "\n";
                        }
                        else {
                            // Если не staged, то удаляем существующее ребро
                            graphManager.removeNeighbor(activeNodeId, target);
                            std::cout << "Removed edge between "
                                << activeNodeId << " and " << target << "\n";
                        }
                    }
                    else {
                        // Если не neighborMode — обычное удаление узла
                        std::cout << "Removing node " << target << "\n";
                        graphManager.removeNodeById(target);
                    }
                }
                return;
            }

            // ====== ALT + ПКМ ======
            // ALT+ПКМ → neighborMode или портал‑завершение
            if (keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT]) {
                std::string target;
                const auto& nodesHere =
                    (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
                    : graphManager.getActiveNodes();
                for (auto& [id, node] : nodesHere) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25) { target = id; break; }
                }
                if (!target.empty()) {
                    if (!portalStartNode.empty() && portalStartNode != target) {
                        // Завершение портала
                        Transition tr{ portalStartNode, target, TransitionType::Door };
                        graphManager.addTransition(tr);
                        std::cout << "Portal created: " << portalStartNode << " <-> " << target << "\n";
                        portalStartNode.clear();
                    }
                    else {
                        // Enter neighborMode
                        activeNodeId = target;
                        neighborMode = true;
                        pendingNeighbors.clear();
                        std::cout << "Neighbor mode started for " << target << "\n";
                    }
                    return;
                }
            }

            // P + ЛКМ → редактирование порталов
            if (keys[SDL_SCANCODE_P]) {
                std::string target;
                const auto& nodesHere =
                    (currentView == ViewMode::Campus) ? graphManager.getCampusNodes()
                    : graphManager.getActiveNodes();
                for (auto& [id, node] : nodesHere) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25) { target = id; break; }
                }
                if (!target.empty()) {
                    if (portalStartNode.empty()) {
                        portalStartNode = target;
                        std::cout << "Portal start: " << portalStartNode << "\n";
                    }
                    else {
                        Transition tr{ portalStartNode, target, TransitionType::Door };
                        graphManager.addTransition(tr);
                        std::cout << "Portal created: " << portalStartNode << " <-> " << target << "\n";
                        portalStartNode.clear();
                    }
                }
                return;
            }

            // ====== ПКМ (в neighborMode) ======
            // Добавление staged соседа
            if (neighborMode && !activeNodeId.empty()) {
                for (auto& [id, node] : graphManager.getActiveNodesMutable()) {
                    SDL_Point scr = camera.worldToScreen({ node.x, node.y });
                    int dx = scr.x - clickScreen.x;
                    int dy = scr.y - clickScreen.y;
                    if (dx * dx + dy * dy <= 25 && id != activeNodeId) {
                        if (std::find(pendingNeighbors.begin(), pendingNeighbors.end(), id) == pendingNeighbors.end()) {
                            pendingNeighbors.push_back(id);
                            std::cout << "Staged neighbor "
                                << id << " for " << activeNodeId << "\n";
                        }
                        return;
                    }
                }
            }

            // ====== Обычный ПКМ ======
            // Создать новый узел
            graphManager.addNode(clickWorld.x, clickWorld.y);
        }
    }

    if (event.type == SDL_KEYDOWN) {
        if (Config::DEV_MODE) {
            if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
                if (neighborMode && !activeNodeId.empty()) {
                    // Подтверждаем все staged соседи
                    for (auto& nb : pendingNeighbors) {
                        graphManager.addNeighbor(activeNodeId, nb);
                    }

                    std::cout << "Neighbor mode ended for "
                        << activeNodeId << ", saved "
                        << pendingNeighbors.size() << " neighbors\n";
                }

                // В любом случае выходим из режима
                neighborMode = false;
                activeNodeId.clear();
                pendingNeighbors.clear();
            }

            // TAB → переключить Campus <-> последний корпус
            if (event.key.keysym.sym == SDLK_TAB) {
                if (currentView == ViewMode::Campus) {
                    // Если раньше был выбран корпус — вернуться туда
                    if (!currentBuilding.empty())
                        switchToFloor(currentBuilding, currentFloor);
                }
                else {
                    currentView = ViewMode::Campus;
                    graphManager.setActiveGraph("__campus");
                    loadMap(Config::CAMPUS_MAP_PATH);
                    std::cout << "Switched to CAMPUS\n";
                }
            }
            if (event.key.keysym.sym == SDLK_LEFT) {
                if (currentBuilding == "Building_C") switchToFloor("Building_B", 1);
                else if (currentBuilding == "Building_B") switchToFloor("Building_A", 1);
                else switchToFloor("Building_C", 1);
            }
            if (event.key.keysym.sym == SDLK_UP) {
                const BuildingMeta* bm = graphManager.getBuildingMeta(currentBuilding);
                if (bm) {
                    for (int i = 0; i < (int)bm->floors.size(); i++) {
                        if (bm->floors[i].floor == currentFloor && i + 1 < (int)bm->floors.size()) {
                            switchToFloor(currentBuilding, bm->floors[i + 1].floor);
                            break;
                        }
                    }
                }
            }
            if (event.key.keysym.sym == SDLK_DOWN) {
                const BuildingMeta* bm = graphManager.getBuildingMeta(currentBuilding);
                if (bm) {
                    for (int i = 0; i < (int)bm->floors.size(); i++) {
                        if (bm->floors[i].floor == currentFloor && i > 0) {
                            switchToFloor(currentBuilding, bm->floors[i - 1].floor);
                            break;
                        }
                    }
                }
            }

            // L - для черчения прямых линий под определенным углом
            if (Config::DEV_MODE && event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_l) {
                    lineMode = !lineMode;
                    lineStartSet = false;
                    std::cout << (lineMode ? "Line mode ENABLED" : "Line mode DISABLED") << "\n";
                }
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
                if (Config::DEV_MODE) graphManager.undoGlobal();
            }
            if ((event.key.keysym.sym == SDLK_y) && (SDL_GetModState() & KMOD_CTRL)) {
                std::cout << "CTRL+Y Redo\n";
                if (Config::DEV_MODE) graphManager.redoGlobal();
            }
            if ((event.key.keysym.sym == SDLK_s) && (SDL_GetModState() & KMOD_CTRL)) {
                if (Config::DEV_MODE) {
                    std::cout << "CTRL+S QuickSave\n";
                    lastSaveTick = SDL_GetTicks(); // запомнить время сохранения
                    graphManager.saveActive();
                }
            }
            if ((event.key.keysym.sym == SDLK_s) && (SDL_GetModState() & KMOD_CTRL)) {
                if (SDL_GetModState() & KMOD_SHIFT) {
                    // СохраняемTransitions
                    std::cout << "CTRL+SHIFT+S Save transitions\n";
                    graphManager.saveTransitions("assets/transitions/transitions.json");
                } else {
                    std::cout << "CTRL+S Save graph\n";
                    graphManager.saveActive();
                }
            }
        }
    }

    // Стрелки для переключения корпусов и этажей
    if (event.key.keysym.sym == SDLK_RIGHT) {
        if (currentBuilding == "Building_A") switchToFloor("Building_B", 1);
        else if (currentBuilding == "Building_B") switchToFloor("Building_C", 1);
        else switchToFloor("Building_A", 1);
    }

    // --- USER/DEV: TEXT INPUT (autocomplete fields) ---
    if (event.type == SDL_TEXTINPUT) {
        if (editingFrom) inputFrom += event.text.text;
        else inputTo += event.text.text;
        selectedSuggestionIndex = -1; // === NEW === Сброс выбора при наборе
    }

    // --- USER/DEV: KEYBOARD control for input fields + suggestions ---
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_TAB:
            editingFrom = !editingFrom;
            selectedSuggestionIndex = -1; // сброс подсветки
            break;

        case SDLK_BACKSPACE:
            if (editingFrom && !inputFrom.empty()) inputFrom.pop_back();
            if (!editingFrom && !inputTo.empty()) inputTo.pop_back();

            // === NEW: если поле стало пустым, чистим подсказки ===
            if ((editingFrom && inputFrom.empty()) || (!editingFrom && inputTo.empty())) {
                currentSuggestions.clear();
                selectedSuggestionIndex = -1;
            }
            else {
                selectedSuggestionIndex = -1; // если просто удалили часть — сброс выбора
            }
            break;

            // === NEW navigation in suggestions ===
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
                // подставляем выбранный alias
                if (editingFrom) inputFrom = currentSuggestions[selectedSuggestionIndex];
                else inputTo = currentSuggestions[selectedSuggestionIndex];
                selectedSuggestionIndex = -1;
            }
            else if (!inputFrom.empty() && !inputTo.empty()) {
                buildPathFromAliases(inputFrom, inputTo);
            }
            break;
        }

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

void MapViewer::onWindowResized(int w, int h) {
    camera.setViewportSize(w, h);
}

MapViewer::~MapViewer() {
    if (mapTexture) SDL_DestroyTexture(mapTexture);
    if (font) TTF_CloseFont(font);
}