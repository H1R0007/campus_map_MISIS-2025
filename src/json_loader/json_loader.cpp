#include "json_loader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Graph JsonLoader::loadGraph(const std::string& filename) {
    Graph graph;

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл: " + filename);
    }

    json j;
    file >> j;

    for (auto& nodeData : j["nodes"]) {
        std::string id = nodeData["id"];
        int x = nodeData["x"];
        int y = nodeData["y"];

        std::vector<std::string> neighbors = nodeData["neighbors"].get<std::vector<std::string>>();

        graph.loadNode(id, x, y, neighbors);
    }

    return graph;
}