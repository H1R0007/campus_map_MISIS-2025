#pragma once
#include <string>
#include <unordered_map>
#include "Node.hpp"

enum class ActionType {
    AddNode,
    RemoveNode,
    AddNeighbor,
    RemoveNeighbor
};

struct Action {
    ActionType type;
    Node nodeCopy;                  // для Add/Remove node
    std::string nodeId;             // основной ID
    std::string neighborId;         // если AddNeighbor/RemoveNeighbor
};

class Graph {
public:
    bool loadFromJson(const std::string& path);
    bool saveToJson(const std::string& path) const;
    void loadNode(const std::string& id, int x, int y, const std::vector<std::string>& neighbors);
    void addNode(int x, int y);

    void removeLastNode();
    void removeNodeById(const std::string& nodeId, bool trackHistory = true);

    void addNeighbor(const std::string& nodeId, const std::string& neighborId);
    void removeNeighbor(const std::string& nodeId, const std::string& neighborId);

    void undo();
    void redo();

    const Node* getNode(const std::string& id) const;
    const std::unordered_map<std::string, Node>& getNodes() const { return nodes; }
    std::unordered_map<std::string, Node>& getNodesMutable() { return nodes; }

private:
    std::unordered_map<std::string, Node> nodes;
    std::string jsonPath; // запоминаем путь загрузки
    int nextId = 1;

    std::vector<Action> undoStack;
    std::vector<Action> redoStack;

    void pushAction(const Action& action) {
        undoStack.push_back(action);
        redoStack.clear();
    }
};

