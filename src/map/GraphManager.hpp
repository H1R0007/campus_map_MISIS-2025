#pragma once
#include "Graph.hpp"
#include "TransitionManager.hpp"
#include "../history/HistoryManager.hpp"
#include "BuildingMeta.hpp"
#include <unordered_map>
#include <string>
#include <memory>

class GraphManager {
public:
    // ====== Загрузка данных ======
    bool loadCampus(const std::string& path);                // campus/graph.json
    bool loadBuildingFloor(const std::string& key, const std::string& path);
    bool loadCampusMeta(const std::string& path);
    bool loadBuildingWithMeta(const std::string& buildingId, const std::string& metaPath);
    bool loadTransitions(const std::string& path);
    const BuildingMeta* getBuildingMeta(const std::string& id) const;

    // ====== Навигация ======
    const Node* getNode(const std::string& id) const;
    std::vector<std::string> getNeighbors(const std::string& id) const;

    // ====== Редактирование ======
    void setActiveGraph(const std::string& key);
    void addNode(int x, int y, int floor = 0);
    void removeNodeById(const std::string& id);
    void addNeighbor(const std::string& a, const std::string& b);
    void removeNeighbor(const std::string& a, const std::string& b);

    void addTransition(const Transition& t);
    void removeTransition(const std::string& from, const std::string& to);
    void saveTransitions(const std::string& path);

    std::vector<std::string> addLinearNodes(int startX, int startY, float angleDeg, int count, int floor = 0);

    void saveActive();

    const std::unordered_map<std::string, Node>& getActiveNodes() const;
    const std::unordered_map<std::string, Node>& getCampusNodes() const;
    std::unordered_map<std::string, Node>& getActiveNodesMutable();

    std::optional<TransitionType> getTransitionType(const std::string& a, const std::string& b) const;

    // ====== Undo/Redo ======
    void undoGlobal();
    void redoGlobal();

    HistoryManager history;

    std::unordered_map<std::string, std::string> graphPaths;
    std::unordered_map<std::string, BuildingMeta> buildingMetas;

private:
    Graph campusGraph;
    std::unordered_map<std::string, Graph> graphs;
    TransitionManager transitions;

    std::string activeKey; // "__campus" или "Building_A_floor_1" и т.д.

    void restoreNodeFromJson(const std::string& jsonData);

    // Уникальный глобальный счётчик ID
    int globalNodeCounter = 1;
};