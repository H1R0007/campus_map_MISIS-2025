#include "Map_Viewer.hpp"
#include "MapRenderer.hpp"
#include "../Input/InputHandler.hpp"
#include "../config.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// === Lifecycle ===
MapViewer::MapViewer(SDL_Renderer* renderer) {
    // 1. Создаем компоненты-помощники
    mapRenderer = std::make_unique<MapRenderer>(renderer);
    inputHandler = std::make_unique<InputHandler>(*this, camera, graphManager);

    // 2. Загружаем все необходимые данные
    graphManager.loadCampus(Config::CAMPUS_GRAPH_PATH);
    graphManager.loadCampusMeta(Config::CAMPUS_META_PATH);
    graphManager.loadTransitions(Config::TRANSITIONS_PATH);
    graphManager.recalculateGlobalNextId();
    aliasManager.load(Config::ALIASES_PATH);

    // 3. Устанавливаем начальное состояние приложения
    switchViewToCampus(); // Устанавливаем вид на кампус
    camera.setWorldSize(Config::CANVAS_WIDTH, Config::CANVAS_HEIGHT);

    SDL_Delay(500);
}

MapViewer::~MapViewer() {}

void MapViewer::onWindowResized(int w, int h) {
    camera.setViewportSize(w, h);
}

void MapViewer::handleEvent(SDL_Event& event) {
    // Вся обработка событий теперь делегируется InputHandler'у
    inputHandler->handleEvent(event);
}

// === Rendering ===
void MapViewer::render() {

    SDL_Renderer* r = mapRenderer->getRenderer();

    // Анимация плавного фокуса камеры (если активна)
    {
        ImGuiIO& io = ImGui::GetIO();
        updateCameraFocus(io.DeltaTime);
    }

    // Цвет фона
    SDL_SetRenderDrawColor(r, 10, 15, 25, 255); // тёмно‑синий фон
    SDL_RenderClear(r);

    std::string viewStr = (currentView == ViewMode::Campus) ? "campus" : "floor";
    mapRenderer->renderScene(
        camera,
        graphManager,
        viewStr,
        currentFloor,
        currentBuilding,
        currentPath,
        hoveredNode,
        activeNodeId,
        pendingNeighbors,
        portalStartNode,
        Config::DEV_MODE,
        debugDrawNodes,
        neighborMode,
        lineToolActive && lineToolFirstPointSet, // Рисуем только если первая точка установлена
        lineToolStart,
        lineToolEnd,
        resolvedFromId,
        resolvedToId
    );
}

// === Actions (called from InputHandler) ===
void MapViewer::buildPathFromAliases(const std::string& startName, const std::string& endName) {

    std::string id1 = aliasManager.resolve(startName);
    std::string id2 = aliasManager.resolve(endName);

    // Если поиск был по ID, а не по алиасу, используем их напрямую
    if (id1.empty() && graphManager.getNode(startName)) id1 = startName;
    if (id2.empty() && graphManager.getNode(endName)) id2 = endName;

    if (id1.empty() || id2.empty()) {
        std::cout << "Pathfinding error: Could not resolve '" << startName << "' or '" << endName << "' to a node ID.\n";
        currentPath.clear();
        return;
    }

    PathFinderOptions opts;
    opts.allowStairs = userAllowStairs;
    opts.allowLift = userAllowLift;
    opts.allowBridge = userAllowBridge;

    currentPath = find_shortest_path(id1, id2, graphManager, opts);

    if (currentPath.empty()) {
        std::cout << "Path not found between " << startName << " and " << endName << "\n";
    }
    else {
        std::cout << "Path built: " << startName << " -> " << endName << " (" << currentPath.size() << " steps)\n";
    }
}

void MapViewer::switchToFloor(const std::string& buildingId, int floor) {
    const BuildingMeta* bm = graphManager.getBuildingMeta(buildingId);
    if (!bm) {
        std::cout << "Error: Building with ID '" << buildingId << "' not found.\n";
        return;
    }

    for (const auto& f : bm->floors) {
        if (f.floor == floor) {
            currentView = ViewMode::BuildingFloor;
            currentBuilding = buildingId;
            currentFloor = f.floor;

            graphManager.setActiveGraph(buildingId + "_floor_" + std::to_string(f.floor));
            loadMap("assets/buildings/" + bm->id + "/" + f.mapPath);

            std::cout << "Switched to " << bm->name << " floor " << floor << "\n";
            return;
        }
    }
    std::cout << "Warning: Floor " << floor << " not found in building " << buildingId << "\n";
}

void MapViewer::switchViewToCampus() {
    currentView = ViewMode::Campus;
    currentBuilding.clear();
    currentFloor = 0;
    graphManager.setActiveGraph("__campus");
    loadMap(Config::CAMPUS_MAP_PATH);
    std::cout << "Switched to CAMPUS view\n";
}

// === Private Helpers ===
void MapViewer::loadMap(const std::string& path) {
    if (!mapRenderer->loadMapTexture(path)) {
        std::cerr << "MapViewer: Failed to load map texture via MapRenderer for path: " << path << std::endl;
    }
}

void MapViewer::createNodeLine(const SDL_Point& startWorld, float angleDeg, int count, int step) {
    if (count <= 1) {
        std::cout << "[LineTool] Node count must be >= 2.\n";
        return;
    }

    float rad = angleDeg * static_cast<float>(M_PI) / 180.0f;
    float effectiveStep = static_cast<float>(step);

    if (lineToolDistance > 0 && count > 1) {
        effectiveStep = lineToolDistance / (count - 1);
    }

    float dx = std::cos(rad) * effectiveStep;
    float dy = std::sin(rad) * effectiveStep;

    std::vector<std::string> newIds;
    for (int i = 0; i < count; ++i) {
        int nx = static_cast<int>(std::round(startWorld.x + dx * i));
        int ny = static_cast<int>(std::round(startWorld.y + dy * i));

        // Ограничиваем координаты границами холста
        nx = std::clamp(nx, 0, Config::CANVAS_WIDTH);
        ny = std::clamp(ny, 0, Config::CANVAS_HEIGHT);

        std::string id = graphManager.addNode(nx, ny, currentFloor);
        newIds.push_back(id);
    }

    // Соединяем созданные узлы в цепочку
    for (size_t i = 0; i < newIds.size() - 1; ++i) {
        graphManager.addNeighbor(newIds[i], newIds[i + 1]);
    }

    std::cout << "[LineTool] Built a line of " << newIds.size() << " nodes.\n";
}

void MapViewer::updateSuggestions() {
    // 1) Текущий активный ввод (из какого поля читаем подсказки)
    std::string& activeInput = editingFrom ? inputFrom : inputTo;

    // 2) Всегда поддерживаем resolvedFromId/resolvedToId (это дёшево)
    auto resolveToId = [&](const std::string& text) -> std::string {
        if (text.empty()) return "";
        // Сначала через алиасы
        std::string id = aliasManager.resolve(text);
        if (!id.empty()) return id;
        // Потом — если ввели прямой id
        if (graphManager.getNode(text)) return text;
        return "";
        };
    resolvedFromId = resolveToId(inputFrom);
    resolvedToId = resolveToId(inputTo);

    // 3) Пересчитываем подсказки только когда строка реально изменилась
    bool needRecompute = false;
    if (editingFrom) {
        if (activeInput != lastSuggestFrom) {
            lastSuggestFrom = activeInput;
            needRecompute = true;
        }
    }
    else {
        if (activeInput != lastSuggestTo) {
            lastSuggestTo = activeInput;
            needRecompute = true;
        }
    }

    if (needRecompute) {
        currentSuggestions.clear();
        if (!activeInput.empty()) {
            currentSuggestions = aliasManager.suggest(activeInput, 3);
        }
    }

    // 4) Мягкое авто‑наведение камеры на резолвнутый маркер (только USER режим)
    bool allowAutoFocus =
#ifdef EMSCRIPTEN
        true;                // в вебе — всегда включаем (мягко и ненавязчиво)
#else
        !Config::DEV_MODE;   // на десктопе — только в USER режиме
#endif

    if (allowAutoFocus) {
        const std::string& curId = editingFrom ? resolvedFromId : resolvedToId;
        const std::string& prevId = editingFrom ? prevResolvedFromId : prevResolvedToId;
        if (!curId.empty() && curId != prevId) {
            requestFocusToNode(curId, 0.28f);
        }
    }

    // 5) Обновляем "предыдущие" значения для де‑баунса фокуса
    prevResolvedFromId = resolvedFromId;
    prevResolvedToId = resolvedToId;
}

void MapViewer::requestFocusToNode(const std::string& nodeId, float durationSec, bool force) {
    // В USER-режиме — всегда можно; в DEV — только если force == true
#ifndef EMSCRIPTEN
    if (Config::DEV_MODE && !force) return;
#endif

    const Node* n = graphManager.getNode(nodeId);
    if (!n) return;

    // Проверим видимость на текущем представлении
    bool visible = false;
    std::string nbld = n->building.empty() ? "CAMPUS" : n->building;
    if (currentView == ViewMode::Campus) {
        visible = (nbld == "CAMPUS");
    }
    else {
        visible = (nbld == currentBuilding && n->floor == currentFloor);
    }
    if (!visible) return;

    // Текущий центр камеры в мировых координатах
    SDL_Point curC = camera.getCenterWorld();
    SDL_FPoint start{ (float)curC.x, (float)curC.y };
    SDL_FPoint target{ (float)n->x, (float)n->y };

    // Если уже почти в цели — не дёргаем
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    if ((dx * dx + dy * dy) < 9.0f) return;

    focusAnim.active = true;
    focusAnim.start = start;
    focusAnim.target = target;
    focusAnim.t = 0.f;
    focusAnim.duration = std::max(0.12f, durationSec);
}

void MapViewer::focusNodeByIdSmart(const std::string& idOrAlias,
    bool switchViewIfNeeded,
    bool adjustZoom,
    float targetZoom)
{
    // 1) Резолвим алиас → id или используем введённый id
    std::string id = aliasManager.resolve(idOrAlias);
    if (id.empty() && graphManager.getNode(idOrAlias)) id = idOrAlias;
    if (id.empty()) {
        std::cout << "[DevDock] Node not found for: " << idOrAlias << "\n";
        return;
    }

    const Node* n = graphManager.getNode(id);
    if (!n) return;

    // 2) По необходимости переключаем вид (кампус/этаж)
    std::string nbld = n->building.empty() ? "CAMPUS" : n->building;
    if (switchViewIfNeeded) {
        if (nbld == "CAMPUS" && currentView != ViewMode::Campus) {
            switchViewToCampus();
        }
        else if (nbld != "CAMPUS") {
            if (currentView != ViewMode::BuildingFloor || currentBuilding != nbld || currentFloor != n->floor) {
                switchToFloor(nbld, n->floor);
            }
        }
    }

    // 3) Опционально подстраиваем зум (не агрессивно)
    if (adjustZoom) {
        float tz = std::clamp(targetZoom, Config::MIN_ZOOM, Config::MAX_ZOOM);
        camera.setScale(tz);
    }

    // 4) Мягкое наведение (force=true, чтобы работало и в DEV)
    requestFocusToNode(id, 0.28f, true);
}

void MapViewer::updateCameraFocus(float dt) {
    if (!focusAnim.active) return;
    focusAnim.t += dt;
    float a = std::min(focusAnim.t / focusAnim.duration, 1.0f);
    // ease-out: 1 - (1 - a)^3
    float u = 1.0f - (1.0f - a) * (1.0f - a) * (1.0f - a);
    SDL_FPoint cur{
        focusAnim.start.x + (focusAnim.target.x - focusAnim.start.x) * u,
        focusAnim.start.y + (focusAnim.target.y - focusAnim.start.y) * u
    };
    camera.setCenterWorld(SDL_Point{ (int)std::lround(cur.x), (int)std::lround(cur.y) });
    if (a >= 1.0f) focusAnim.active = false;
}