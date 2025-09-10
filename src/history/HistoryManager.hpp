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
    SwitchView
};

struct HistoryAction {
    ActionType type;
    std::string data1; // универсальные ID (nodeId, fromId и т.п.)
    std::string data2;
    std::string extra; // поле для расширений (json-строки, алиасы и т.п.)
};

class HistoryManager {
public:
    void push(const HistoryAction& action);
    std::optional<HistoryAction> undo();
    std::optional<HistoryAction> redo();

    void clear();
    void dumpToFile(const std::string& path) const; // dev_log.json

private:
    std::vector<HistoryAction> undoStack;
    std::vector<HistoryAction> redoStack;
};