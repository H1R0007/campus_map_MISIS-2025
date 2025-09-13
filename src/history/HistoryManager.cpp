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
        json entry;
        entry["type"] = (int)a.type;
        entry["data1"] = a.data1;
        entry["data2"] = a.data2;

        // Если extra — это JSON, пробуем его считать как JSON
        try {
            entry["extra"] = json::parse(a.extra);
        }
        catch (...) {
            entry["extra"] = a.extra; // как строку
        }

        j["history"].push_back(entry);
    }

    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(4);
    }
}