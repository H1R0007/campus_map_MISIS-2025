#pragma once
#include <string>
#include <unordered_map>
#include "Node.hpp"

class Graph {
public:
    bool loadFromJson(const std::string& path);
    bool saveToJson(const std::string& path) const;

    void addNode(int x, int y);
    void removeLastNode();

    const Node* getNode(const std::string& id) const;
    const std::unordered_map<std::string, Node>& getNodes() const { return nodes; }
    std::unordered_map<std::string, Node>& getNodesMutable() { return nodes; }

private:
    std::unordered_map<std::string, Node> nodes;
    std::string jsonPath; // запоминаем путь загрузки
    int nextId = 1;
};