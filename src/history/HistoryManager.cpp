// src/history/HistoryManager.cpp

#include "HistoryManager.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void HistoryManager::push(const HistoryAction& action) {
    undoStack.push_back(action);
    redoStack.clear(); // после нового действия redo теряет смысл
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
    f << j.dump(4);
}