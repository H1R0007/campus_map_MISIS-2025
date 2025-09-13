// src/history/HistoryManager.hpp

#pragma once
#include <string>
#include <vector>
#include <optional>

// === ActionType ===
// Describes type of edit operation stored in history.
// These are invoked during undo/redo.
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

// === HistoryAction ===
// Represents a single undo/redo step.
// Holds IDs and optional metadata (extra JSON, alias string, etc.).
struct HistoryAction {
    ActionType type;
    std::string data1; // universal IDs: nodeId, fromId, etc.
    std::string data2;
    std::string extra; // optional payload (e.g. JSON snapshot)
};

// === HistoryManager ===
// Simple undo/redo manager for editor-like operations.
// Keeps two stacks: undo and redo.
class HistoryManager {
public:
    HistoryManager() = default;

    // === Core API ===
    void push(const HistoryAction& action);

    std::optional<HistoryAction> undo();
    std::optional<HistoryAction> redo();

    // === Utility ===
    void clear();
    void dumpToFile(const std::string& path) const; // export stack to JSON file

private:
    std::vector<HistoryAction> undoStack;
    std::vector<HistoryAction> redoStack;
};