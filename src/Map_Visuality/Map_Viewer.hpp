#pragma once
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <vector>

#include "../Camera/Camera.hpp"
#include "../path_finder/path_finder.hpp"
#include "../aliases/AliasManager.hpp"
#include "../map/GraphManager.hpp"

// Вперед объявляем классы-помощники
class MapRenderer;
class InputHandler;

// === ViewMode ===
// Определяет, что сейчас отображается: карта всего кампуса или этаж здания.
enum class ViewMode { Campus, BuildingFloor };

// === MapViewer ===
// Главный класс-координатор. Хранит состояние приложения,
// делегирует отрисовку классу MapRenderer, а обработку ввода - InputHandler.
class MapViewer {
public:
    MapViewer(SDL_Renderer* renderer);
    ~MapViewer();

    // === Жизненный цикл ===
    void onWindowResized(int w, int h);
    void handleEvent(SDL_Event& event);

    // === Отрисовка ===
    void render();        // Отрисовка основной сцены (карта, узлы, пути)

    // === Методы-действия (вызываются из InputHandler) ===
    void buildPathFromAliases(const std::string& startName, const std::string& endName);
    void switchToFloor(const std::string& buildingId, int floor);
    void switchViewToCampus();
    void createNodeLine(const SDL_Point& startWorld, float angleDeg, int count, int step);

    // === Getters (для доступа из других классов) ===
    GraphManager& getGraphManager() { return graphManager; }
    std::string& getInputFrom() { return inputFrom; }
    std::string& getInputTo() { return inputTo; }
    const std::vector<std::string>& getCurrentSuggestions() const { return currentSuggestions; }
    bool& getUserAllowStairs() { return userAllowStairs; }
    bool& getUserAllowLift() { return userAllowLift; }
    bool& getUserAllowBridge() { return userAllowBridge; }

    // Access to debug info for the UI
    const SDL_Point& getDebugMouseWorld() const { return debugMouseWorld; }
    const Camera& getCamera() const { return camera; }

    // === Accessors for external managers ===
    AliasManager& getAliasManager() { return aliasManager; }
    const AliasManager& getAliasManager() const { return aliasManager; }

    // === Accessors for current building and floor (used by UI) ===
    const std::string& getCurrentBuilding() const { return currentBuilding; }
    int getCurrentFloor() const { return currentFloor; }

    // Access to DEV mode state for the UI
    const std::string& getInspectorNodeId() const { return inspectorNodeId; }
    const std::string& getNeighborModeActiveId() const { return activeNodeId; }
    bool isNeighborModeActive() const { return neighborMode; }
    Uint32 getLastSaveTick() const { return lastSaveTick; }

   /* bool isTransitionActive() const { return transitionActive; }
    float getTransitionProgress() const {
        if (!transitionActive) return 0.0f;
        float elapsed = (SDL_GetTicks() - transitionStart) / 1000.0f;
        return std::min(elapsed / 0.6f, 1.0f);
    }
    void completeTransition() { transitionActive = false; transitionAlpha = 0.0f; targetMapPath.clear(); }
    const std::string& getTargetMapPath() const { return targetMapPath; }
    void loadTargetMap() { if (!targetMapPath.empty()) loadMap(targetMapPath); }
    SDL_Renderer* getRenderer();*/

    // Setters and getters for UI state
    void setEditingFrom(bool isEditingFrom) { editingFrom = isEditingFrom; }
    bool isEditingFrom() const { return editingFrom; }

    // Method to manually clear suggestions
    void clearSuggestions() { currentSuggestions.clear(); }

    // Метод для обновления подсказок, который будет вызываться каждый кадр
    void updateSuggestions();

private:
    // Даем классам-помощникам доступ к приватному состоянию этого класса
    friend class InputHandler;
    friend class MapRenderer;

    // --- Компоненты ---
    GraphManager graphManager;
    AliasManager aliasManager;
    Camera camera;
    std::unique_ptr<MapRenderer> mapRenderer;
    std::unique_ptr<InputHandler> inputHandler;

    // --- Состояние вида и навигации ---
    ViewMode currentView = ViewMode::Campus;
    std::string currentBuilding;
    int currentFloor = 0;
    std::vector<std::string> currentPath;

    // --- Состояние DEV-режима и инструментов ---
    SDL_Point debugMouseWorld{ 0,0 };
    const Node* hoveredNode = nullptr;
    std::string activeNodeId;
    bool neighborMode = false;
    std::vector<std::string> pendingNeighbors;
    std::string portalStartNode;
    std::string inspectorNodeId;
    std::string startNodeId; // Для выбора начальной точки пути в DEV-режиме
    std::string endNodeId;   // Для выбора конечной точки пути в DEV-режиме

    // Инструмент "Линия"
    bool lineToolActive = false;
    bool lineToolFirstPointSet = false;
    SDL_Point lineToolStart{};
    SDL_Point lineToolEnd{};
    float lineToolAngleDeg = 0.0f;
    float lineToolDistance = 0.0f;
    int lineToolCount = 5;
    int lineToolStep = 100;
    bool lineToolReady = false;

    // --- Состояние UI (поиск маршрута) ---
    std::string inputFrom, inputTo;
    bool editingFrom = true;
    int selectedSuggestionIndex = -1;
    std::vector<std::string> currentSuggestions;
    bool inputActive = false;
    SDL_Rect fromFieldRect{}, toFieldRect{}; // Области для кликов по полям ввода
    Uint32 lastSaveTick = 0;

    // --- Настройки пользователя и отладки ---
    bool userAllowStairs = true;
    bool userAllowLift = true;
    bool userAllowBridge = true;
    bool debugDrawNodes = true; // Показывать/скрывать узлы в DEV-режиме

    // --- Плавные переходы между этажами/корпусами ---
    /*bool transitionActive = false;
    float transitionAlpha = 0.0f;
    Uint32 transitionStart = 0;
    std::string targetMapPath;*/

    // --- Приватные хелперы, используемые внутри класса ---
    void loadMap(const std::string& path);

    // Поиск узла под курсором. Используется InputHandler'ом для определения цели клика.
    // Возвращает ID узла или пустую строку.
    std::string findNodeUnderCursor(const SDL_Point& clickScreen, int radius = 15) const {
        const auto& nodesHere = (currentView == ViewMode::Campus)
            ? graphManager.getCampusNodes()
            : graphManager.getActiveNodes();

        for (const auto& [id, node] : nodesHere) {
            SDL_Point scr = camera.worldToScreen({ node.x, node.y });
            int dx = scr.x - clickScreen.x;
            int dy = scr.y - clickScreen.y;
            if (dx * dx + dy * dy <= radius * radius) {
                return id;
            }
        }
        return "";
    }
};