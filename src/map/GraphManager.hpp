#pragma once
#include "Graph.hpp"
#include "TransitionManager.hpp"
#include "../history/HistoryManager.hpp"
#include "BuildingMeta.hpp"
#include <unordered_map>
#include <string>
#include <memory>

// === GraphManager ===
// High-level facade coordinating all graphs (campus, building floors).
// Handles editing operations, undo/redo, saving, transitions.
class GraphManager {
public:
    GraphManager() = default;

    // === Data Loading ===
    bool loadCampus(const std::string& path);                // campus/graph.json
    bool loadCampusMeta(const std::string& path);            // campus/meta.json
    bool loadBuildingWithMeta(const std::string& buildingId, const std::string& metaPath);
    bool loadBuildingFloor(const std::string& key, const std::string& path);
    bool loadTransitions(const std::string& path);

    // === Metadata ===
    const BuildingMeta* getBuildingMeta(const std::string& id) const;
    std::unordered_map<std::string, BuildingMeta> buildingMetas;

    // === Navigation Helpers ===
    const Node* getNode(const std::string& id) const;
    std::vector<std::string> getNeighbors(const std::string& id) const;
    std::optional<TransitionType> getTransitionType(const std::string& a, const std::string& b) const;

    // === Editing (active graph) ===
    void setActiveGraph(const std::string& key); // "__campus" or "Building_X_floor_Y"
    void addNode(int x, int y, int floor = 0);
    void removeNodeById(const std::string& id);
    void addNeighbor(const std::string& a, const std::string& b);
    void removeNeighbor(const std::string& a, const std::string& b);
    void addTransition(const Transition& t);
    void removeTransition(const std::string& from, const std::string& to);

    // Save/load
    void saveActive();                                            // save current graph
    void saveTransitions(const std::string& path);                // save transition list
    std::unordered_map<std::string, std::string> graphPaths;      // key -> path

    // === Node Accessors ===
    const std::unordered_map<std::string, Node>& getActiveNodes() const;
    const std::unordered_map<std::string, Node>& getCampusNodes() const;
    std::unordered_map<std::string, Node>& getActiveNodesMutable();

    // === Undo/Redo ===
    void undoGlobal();
    void redoGlobal();
    HistoryManager history;

private:
    // Internal graphs
    Graph campusGraph;
    std::unordered_map<std::string, Graph> graphs;   // key = "Building_X_floor_Y"
    TransitionManager transitions;

    // Which graph is currently edited ("__campus" or floor key)
    std::string activeKey;

    // Helper to rebuild node from JSON snapshot (used in undo/redo).
    void restoreNodeFromJson(const std::string& jsonData);
};