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
    // Загрузка данных
    bool loadCampus(const std::string& path);                // campus/graph.json
    bool loadBuildingFloor(const std::string& key, const std::string& path);

    bool loadCampusMeta(const std::string& path);
    bool loadBuildingWithMeta(const std::string& buildingId, const std::string& metaPath);

    const BuildingMeta* getBuildingMeta(const std::string& id) const;
    bool loadTransitions(const std::string& path);

    // Навигация
    const Node* getNode(const std::string& id) const;
    std::vector<std::string> getNeighbors(const std::string& id) const;

    // === Редактирование ===
    void setActiveGraph(const std::string& key); // например "Building_B_floor_1"
    void addNode(int x, int y, int floor = 0);
    void removeNodeById(const std::string& id);
    void addNeighbor(const std::string& a, const std::string& b);
    void removeNeighbor(const std::string& a, const std::string& b);
    void addTransition(const Transition& t);
    void removeTransition(const std::string& from, const std::string& to);
    void saveTransitions(const std::string& path);
    std::unordered_map<std::string, std::string> graphPaths;
    const std::unordered_map<std::string, Node>& getActiveNodes() const;
    const std::unordered_map<std::string, Node>& getCampusNodes() const;
    std::unordered_map<std::string, Node>& getActiveNodesMutable();
    std::unordered_map<std::string, BuildingMeta> buildingMetas;
    void saveActive();

    std::optional<TransitionType> getTransitionType(const std::string& a, const std::string& b) const;

    // Новый глобальный undo/redo
    void undoGlobal();
    void redoGlobal();

    HistoryManager history;

private:
    Graph campusGraph;
    std::unordered_map<std::string, Graph> graphs; 
    TransitionManager transitions;

    std::string activeKey; // какой граф редактируем

    void restoreNodeFromJson(const std::string& jsonData);
};