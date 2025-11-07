#include "HistoryManager.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

// === Core API ===
constexpr size_t MAX_HISTORY = 200;
void HistoryManager::push(const HistoryAction& action) {
    if (undoStack.size() > MAX_HISTORY) undoStack.erase(undoStack.begin());
    undoStack.push_back(action);
    redoStack.clear(); // new action invalidates redo stack
}

std::optional<HistoryAction> HistoryManager::undo() {
    if (undoStack.empty()) return std::nullopt;
    auto act = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back(act);
    return act;
}

std::optional<HistoryAction> HistoryManager::redo() {
    if (redoStack.empty()) return std::nullopt;
    auto act = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back(act);
    return act;
}

// === Utility ===
void HistoryManager::clear() {
    undoStack.clear();
    redoStack.clear();
}

void HistoryManager::dumpToFile(const std::string& path) const {
    json j;
    for (auto& a : undoStack) {
        j["history"].push_back({
            {"type", (int)a.type},
            {"data1", a.data1},
            {"data2", a.data2},
            {"extra", a.extra}
            });
    }
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to write history dump: " << path << "\n";
        return;
    }
    f << j.dump(4);
}