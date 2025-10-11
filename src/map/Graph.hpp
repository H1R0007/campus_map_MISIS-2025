#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>  
#include "Node.hpp"


// === Graph ===
// Represents a single navigation graph (campus level or specific building floor).
// Stores all nodes and supports modification, saving, loading.
class Graph {
public:
    Graph() = default;

    // === I/O ===
    // Load graph from JSON file (path is saved into jsonPath).
    bool loadFromJson(const std::string& path);
    
    // Save current graph to JSON file.
    bool saveToJson(const std::string& path) const;

    // === Node operations ===
    // Add a new auto-generated node (id = "node_X").
    std::string addNode(int x, int y, int floor = 0);

    // Load node with explicit id + neighbors
    // (used during JSON parsing and data restore).
    void loadNode(const std::string& id, int x, int y, int floor, const std::vector<std::string>& neighbors);

    // Remove node by id, optionally track history externally.
    void removeNodeById(const std::string& nodeId);

    // Access a node by id (nullptr if missing).
    const Node* getNode(const std::string& id) const;

    // === Neighbor operations ===
    void addNeighbor(const std::string& nodeId, const std::string& neighborId);
    void removeNeighbor(const std::string& nodeId, const std::string& neighborId);
    
    // === Accessors ===
    const std::unordered_map<std::string, Node>& getNodes() const { return nodes; }
    std::unordered_map<std::string, Node>& getNodesMutable() { return nodes; }

private:
    // in-memory node collection
    std::unordered_map<std::string, Node> nodes;

    // path to associated JSON file (used for save/load)
    std::string jsonPath;

    // auto-id counter for "node_X" generation
    int nextId = 1;
};

