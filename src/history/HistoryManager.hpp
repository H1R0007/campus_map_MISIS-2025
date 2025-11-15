// src/history/HistoryManager.hpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

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

// Utility: string conversion (for dumps/logs)
inline const char* ActionTypeToString(ActionType t) {
    switch (t) {
    case ActionType::AddNode:         return "AddNode";
    case ActionType::RemoveNode:      return "RemoveNode";
    case ActionType::AddNeighbor:     return "AddNeighbor";
    case ActionType::RemoveNeighbor:  return "RemoveNeighbor";
    case ActionType::AddTransition:   return "AddTransition";
    case ActionType::RemoveTransition:return "RemoveTransition";
    case ActionType::EditAlias:       return "EditAlias";
    case ActionType::SwitchView:      return "SwitchView";
    default:                          return "Unknown";
    }
}

// === HistoryAction ===
// Represents a single undo/redo step.
// Holds IDs and optional metadata (extra JSON, alias string, etc.).
struct HistoryAction {
    ActionType type = ActionType::SwitchView;
    std::string data1; // universal IDs: nodeId, fromId, etc.
    std::string data2;
    std::string extra; // optional payload (e.g. JSON snapshot)

    // New: human-readable label (optional) and timestamp (ms since epoch)
    std::string label;
    std::uint64_t tsMs = 0; // auto-filled by HistoryManager::push(...)
};

// === HistoryManager ===
// Simple undo/redo manager for editor-like operations.
// Keeps two stacks: undo and redo.
class HistoryManager {
public:
    HistoryManager() = default;

    // === Core API (back-compatible) ===
    void push(const HistoryAction& action);                  // push + clear redo
    std::optional<HistoryAction> undo();                     // pop from undo -> push to redo
    std::optional<HistoryAction> redo();                     // pop from redo -> push to undo

    // === Extras ===
    // Coalescing: try merging with the last action if it is the same kind (e.g. EditAlias)
    void pushCoalesced(const HistoryAction& action, int timeWindowMs = 600);

    // Query
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    size_t undoSize() const { return undoStack.size(); }
    size_t redoSize() const { return redoStack.size(); }

    // Capacity control (applies to both stacks separately)
    void setMaxEntries(size_t max) { maxEntries = (max == 0 ? 1 : max); trimStacksIfNeeded(); }
    size_t getMaxEntries() const { return maxEntries; }

    // Savepoint API: mark moment when data is persisted, then check "dirty" status
    void markSavepoint() { savepointUndoSize = undoStack.size(); savepointRedoSize = 0; }
    bool isAtSavepoint() const { return (undoStack.size() == savepointUndoSize) && redoStack.empty(); }

    // === Utility ===
    void clear();
    void dumpToFile(const std::string& path) const; // export stack to JSON file

private:
    std::vector<HistoryAction> undoStack;
    std::vector<HistoryAction> redoStack;

    size_t maxEntries = 200;           // per-stack entry cap
    size_t savepointUndoSize = 0;      // for "dirty" check
    size_t savepointRedoSize = 0;

    void trimStacksIfNeeded();
    static std::uint64_t nowMs();

    // Coalescing helpers
    static bool canCoalesce(const HistoryAction& prev, const HistoryAction& next);
    static HistoryAction mergeCoalesced(const HistoryAction& prev, const HistoryAction& next);
};