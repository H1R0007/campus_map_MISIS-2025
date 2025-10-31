#include "json_loader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// === Public API ===
Graph JsonLoader::loadGraph(const std::string& filename) {
    Graph graph;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[JsonLoader] Cannot open " << filename << "\n";
        return graph;
    }

    nlohmann::json j;
    try {
        file >> j;
    }
    catch (std::exception& e) {
        std::cerr << "[JsonLoader] Parse error in " << filename << ": " << e.what() << "\n";
        return graph;
    }

    if (!j.contains("nodes") || !j["nodes"].is_array()) {
        std::cerr << "[JsonLoader] Invalid nodes structure in " << filename << "\n";
        return graph;
    }

    for (auto& nodeData : j["nodes"]) {
        Node node;
        node.id = nodeData.value("id", "");
        node.x = nodeData.value("x", 0);
        node.y = nodeData.value("y", 0);
        node.floor = nodeData.value("floor", 0);
        node.neighbors = nodeData.value<std::vector<std::string>>("neighbors", {});
        graph.loadNode(node.id, node.x, node.y, node.floor, node.neighbors);
    }

    return graph;
}