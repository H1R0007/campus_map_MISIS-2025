#include "Graph.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

bool Graph::loadFromJson(const std::string& path) {
    jsonPath = path;

    std::ifstream file(path);
    if (!file.is_open() || file.peek() == std::ifstream::traits_type::eof()) {
        // файл пустой или нет
        std::cerr << "nodes.json missing or empty. Starting with empty graph." << std::endl;
        nodes.clear();
        nextId = 1;
       
        // Создаём базовый JSON { "nodes": [] }
        std::ofstream ofs(path);
        ofs << "{\n  \"nodes\": []\n}\n";
        ofs.close();

        return true;
    }

    json j;
    try {
        file >> j;
    }
    catch (std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        nodes.clear();
        nextId = 1;
        return true; // допускаем пустой старт
    }

    nodes.clear();

    if (!j.contains("nodes") || !j["nodes"].is_array()) {
        std::cerr << "Invalid JSON structure, expected \"nodes\": [...]" << std::endl;
        nodes.clear();
        nextId = 1;
        return true;
    }

    for (auto& n : j["nodes"]) {
        Node node;
        node.id = n.value("id", "");
        node.x = n.value("x", 0);
        node.y = n.value("y", 0);

        if (n.contains("neighbors")) {
            for (auto& nb : n["neighbors"])
                node.neighbors.push_back(nb.get<std::string>());
        }

        try {
            if (node.id.rfind("node_", 0) == 0) {
                int num = std::stoi(node.id.substr(5));
                if (num >= nextId) nextId = num + 1;
            }
        }
        catch (...) {}

        if (!node.id.empty()) {
            nodes[node.id] = std::move(node);
        }
    }

    std::cout << "Loaded " << nodes.size() << " nodes from " << path << std::endl;
    return true;
}

bool Graph::saveToJson(const std::string& path) const {
    json j;
    for (auto& [id, node] : nodes) {
        j["nodes"].push_back({
            {"id", node.id},
            {"x", node.x},
            {"y", node.y},
            {"neighbors", node.neighbors}
            });
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to save " << path << "\n";
        return false;
    }
    file << j.dump(4); // красиво с отступами
    return true;
}

void Graph::addNode(int x, int y) {
    Node node;
    node.id = "node_" + std::to_string(nextId++);
    node.x = x;
    node.y = y;

    nodes[node.id] = node;
    std::cout << "Added " << node.id << " at (" << x << "," << y << ")\n";

    if (!jsonPath.empty()) {
        saveToJson(jsonPath);
    }
}

void Graph::removeLastNode() {
    if (nodes.empty()) {
        std::cout << "No nodes to remove.\n";
        return;
    }

    int maxNum = -1;
    std::string lastId;

    for (auto& [id, node] : nodes) {
        if (id.rfind("node_", 0) == 0) {
            try {
                int num = std::stoi(id.substr(5));
                if (num > maxNum) {
                    maxNum = num;
                    lastId = id;
                }
            }
            catch (...) {}
        }
    }

    if (lastId.empty()) {
        std::cout << "No auto-generated nodes to remove.\n";
        return;  // <-- тут выходим безопасно
    }

    // Удаляем упоминания в neighbors
    for (auto& [id, node] : nodes) {
        node.neighbors.erase(
            std::remove(node.neighbors.begin(), node.neighbors.end(), lastId),
            node.neighbors.end()
        );
    }

    nodes.erase(lastId);
    std::cout << "Removed " << lastId << "\n";

    if (!jsonPath.empty()) {
        saveToJson(jsonPath);
    }
}

const Node* Graph::getNode(const std::string& id) const {
    auto it = nodes.find(id);
    if (it != nodes.end())
        return &it->second;
    return nullptr;
}

void Graph::removeNodeById(const std::string& nodeId, bool trackHistory) {
    auto it = nodes.find(nodeId);
    if (it == nodes.end()) return;

    Node removed = it->second; // сохраняем копию

    // Удаляем из соседей других
    for (auto& [id, node] : nodes) {
        node.neighbors.erase(
            std::remove(node.neighbors.begin(), node.neighbors.end(), nodeId),
            node.neighbors.end()
        );
    }
    nodes.erase(it);

    if (trackHistory) {
        undoStack.push_back({ ActionType::RemoveNode, removed, nodeId, "" });
        redoStack.clear();
    }
    saveToJson(jsonPath);
}

void Graph::undo() {
    if (undoStack.empty()) {
        std::cout << "Nothing to undo.\n";
        return;
    }
    Action act = undoStack.back();
    undoStack.pop_back();

    switch (act.type) {
    case ActionType::AddNode: {
        // отменяем добавление → удаляем
        removeNodeById(act.nodeId, false); // false = не пушить снова в undo
        break;
    }
    case ActionType::RemoveNode: {
        // восстановить удалённый
        nodes[act.nodeCopy.id] = act.nodeCopy;
        break;
    }
    case ActionType::AddNeighbor: {
        // отменяем добавление соседа
        auto& n = nodes[act.nodeId];
        n.neighbors.erase(
            std::remove(n.neighbors.begin(), n.neighbors.end(), act.neighborId),
            n.neighbors.end());
        break;
    }
    case ActionType::RemoveNeighbor: {
        // вернуть соседа обратно
        nodes[act.nodeId].neighbors.push_back(act.neighborId);
        break;
    }
    }
    redoStack.push_back(act);
    saveToJson(jsonPath);
}

void Graph::redo() {
    if (redoStack.empty()) {
        std::cout << "Nothing to redo.\n";
        return;
    }
    Action act = redoStack.back();
    redoStack.pop_back();

    switch (act.type) {
    case ActionType::AddNode: {
        nodes[act.nodeCopy.id] = act.nodeCopy;
        break;
    }
    case ActionType::RemoveNode: {
        removeNodeById(act.nodeId, false);
        break;
    }
    case ActionType::AddNeighbor: {
        nodes[act.nodeId].neighbors.push_back(act.neighborId);
        break;
    }
    case ActionType::RemoveNeighbor: {
        auto& n = nodes[act.nodeId];
        n.neighbors.erase(
            std::remove(n.neighbors.begin(), n.neighbors.end(), act.neighborId),
            n.neighbors.end());
        break;
    }
    }
    undoStack.push_back(act);
    saveToJson(jsonPath);
}
