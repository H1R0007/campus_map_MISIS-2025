#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Node.hpp"
#include <regex>  

// Тип действия для Undo/Redo
enum class ActionType {
    AddNode,
    RemoveNode,
    AddNeighbor,
    RemoveNeighbor
};

struct Action {
    ActionType type;
    Node nodeCopy;         // копия узла (для восстановления)
    std::string nodeId;    // id узла
    std::string neighborId; // сосед, если действие связано с ребром
};

class Graph {
public:
    Graph() = default;

    // Загрузка и сохранение графа
    bool loadFromJson(const std::string& path);
    bool saveToJson(const std::string& path) const;

    // Работа с узлами
    void addNode(int x, int y, int floor = 0);
    void loadNode(const std::string& id, int x, int y, int floor, const std::vector<std::string>& neighbors);
    void removeLastNode();
    void removeNodeById(const std::string& nodeId, bool trackHistory = true);
    const Node* getNode(const std::string& id) const;
    const std::unordered_map<std::string, Node>& getNodes() const { return nodes; }
    std::unordered_map<std::string, Node>& getNodesMutable() { return nodes; }
    void addNodeAuto(int x, int y);

    // Работа с соседями
    void addNeighbor(const std::string& nodeId, const std::string& neighborId);
    void removeNeighbor(const std::string& nodeId, const std::string& neighborId);

    // Undo/Redo
    void undo();
    void redo();

private:
    std::unordered_map<std::string, Node> nodes; // все узлы графа
    std::string jsonPath;                        // путь к JSON-файлу
    int nextId = 1;                              // счётчик для генерации id

    // История изменений для Undo/Redo
    std::vector<Action> undoStack;
    std::vector<Action> redoStack;

    // Вспомогательная функция добавления в стек Undo
    void pushAction(const Action& act) {
        undoStack.push_back(act);
        redoStack.clear(); // сбрасываем Redo после нового действия
    }
};