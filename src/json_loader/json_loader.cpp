#include "json_loader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// === Public API ===
Graph JsonLoader::loadGraph(const std::string& filename) {
    Graph graph;

    std::ifstream file(filename);
    if (!file.is_open()) {
        // Important: we fail explicitly, instead of silently returning empty graph
        throw std::runtime_error("Failed to open file: " + filename);
    }

    // --- JSON parse ---
    json j;
    file >> j;  // note: will throw if JSON structure is broken

    // --- Nodes (JSON array -> Graph nodes) ---
    // Each node expected to contain at least: id, x, y, floor, neighbors.
    for (auto& nodeData : j["nodes"]) {
        std::string id = nodeData["id"];
        int x = nodeData["x"];
        int y = nodeData["y"];
        int floor = nodeData.value("floor", 0); // floor is optional, defaults to 0 if missing

        // neighbors: parsed as array<string>
        std::vector<std::string> neighbors = nodeData["neighbors"].get<std::vector<std::string>>();

        // push node into Graph
        graph.loadNode(id, x, y, floor, neighbors);
    }

    return graph;
}