// src/history/HistoryManager.hpp

#pragma once
#include <string>
#include <vector>
#include <optional>

enum class ActionType {
    AddNode,
    RemoveNode,
    AddNeighbor,
    RemoveNeighbor,
    AddTransition,
    RemoveTransition,
    EditAlias,
    SwitchView,
    AddNodesLine   // Добавление пачки узлов по линии
};

struct HistoryAction {
    ActionType type;
    std::string data1; // универсальные ID (nodeId, fromId и т.п.)
    std::string data2;
    std::string extra; // обычно хранится JSON (snap одного узла или пачки)
};

class HistoryManager {
public:
    void push(const HistoryAction& action);
    std::optional<HistoryAction> undo();
    std::optional<HistoryAction> redo();

    void clear();
    void dumpToFile(const std::string& path) const; // dev_log.json (лог истории!)

private:
    std::vector<HistoryAction> undoStack;
    std::vector<HistoryAction> redoStack;
};