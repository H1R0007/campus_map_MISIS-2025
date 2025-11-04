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
        lineToolEnd
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
    // Determine the currently active input string
    std::string& currentInput = editingFrom ? inputFrom : inputTo;

    currentSuggestions.clear();
    if (!currentInput.empty()) {
        currentSuggestions = aliasManager.suggest(currentInput, 3);
    }
}