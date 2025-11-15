#include "HistoryManager.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>

using json = nlohmann::json;

// === Internal helpers ===
std::uint64_t HistoryManager::nowMs() {
    using clock = std::chrono::system_clock;
    auto now = clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void HistoryManager::trimStacksIfNeeded() {
    // Простой и предсказуемый трим: поддерживаем не более maxEntries в каждом стеке
    while (undoStack.size() > maxEntries) {
        undoStack.erase(undoStack.begin());
        // сдвиг savepoint, если он «проскочил»
        if (savepointUndoSize > 0) savepointUndoSize = std::min(savepointUndoSize, undoStack.size());
    }
    while (redoStack.size() > maxEntries) {
        redoStack.erase(redoStack.begin());
        if (savepointRedoSize > 0) savepointRedoSize = std::min(savepointRedoSize, redoStack.size());
    }
}

// === Coalescing rules ===
// Сейчас объединяем только EditAlias, если один и тот же объект (data1) правится короткими порциями.
// Остальное — не объединяем (безопаснее).
bool HistoryManager::canCoalesce(const HistoryAction& prev, const HistoryAction& next) {
    if (prev.type != next.type) return false;
    if (prev.type == ActionType::EditAlias) {
        // один и тот же объект (например nodeId в data1)
        return (!prev.data1.empty() && prev.data1 == next.data1);
    }
    return false;
}

HistoryAction HistoryManager::mergeCoalesced(const HistoryAction& prev, const HistoryAction& next) {
    HistoryAction r = next;
    // Сохраняем более осмысленный label
    r.label = !next.label.empty() ? next.label : prev.label;
    // extra/вторичные поля — берем из последней правки
    // timestamp обновим при push/pushCoalesced
    return r;
}

// === Core API ===
void HistoryManager::push(const HistoryAction& action) {
    HistoryAction a = action;
    if (a.label.empty()) a.label = ActionTypeToString(a.type);
    a.tsMs = nowMs();

    if (undoStack.size() >= maxEntries) {
        undoStack.erase(undoStack.begin()); // FIX: раньше было >, могло переполнить на +1
        if (savepointUndoSize > 0) savepointUndoSize = std::min(savepointUndoSize, undoStack.size());
    }
    undoStack.push_back(std::move(a));
    redoStack.clear();
    savepointRedoSize = 0;
}

void HistoryManager::pushCoalesced(const HistoryAction& action, int timeWindowMs) {
    HistoryAction a = action;
    if (a.label.empty()) a.label = ActionTypeToString(a.type);
    a.tsMs = nowMs();

    if (!undoStack.empty()) {
        const HistoryAction& last = undoStack.back();
        // В окно слияния, того же типа, разрешено coalesce
        if (canCoalesce(last, a)) {
            if (a.tsMs >= last.tsMs && (a.tsMs - last.tsMs) <= static_cast<std::uint64_t>(timeWindowMs)) {
                HistoryAction merged = mergeCoalesced(last, a);
                merged.tsMs = a.tsMs;
                undoStack.back() = std::move(merged);
                redoStack.clear();
                savepointRedoSize = 0;
                trimStacksIfNeeded();
                return;
            }
        }
    }

    // иначе обычный push
    push(a);
}

std::optional<HistoryAction> HistoryManager::undo() {
    if (undoStack.empty()) return std::nullopt;
    HistoryAction act = std::move(undoStack.back());
    undoStack.pop_back();
    redoStack.push_back(act);
    trimStacksIfNeeded();
    return act;
}

std::optional<HistoryAction> HistoryManager::redo() {
    if (redoStack.empty()) return std::nullopt;
    HistoryAction act = std::move(redoStack.back());
    redoStack.pop_back();
    undoStack.push_back(act);
    trimStacksIfNeeded();
    return act;
}

// === Utility ===
void HistoryManager::clear() {
    undoStack.clear();
    redoStack.clear();
    savepointUndoSize = 0;
    savepointRedoSize = 0;
}

void HistoryManager::dumpToFile(const std::string& path) const {
    json j;
    j["undo"] = json::array();
    j["redo"] = json::array();

    auto dumpStack = [](const std::vector<HistoryAction>& stack) {
        json arr = json::array();
        for (const auto& a : stack) {
            json item = {
                {"type",        (int)a.type},
                {"type_name",   ActionTypeToString(a.type)},
                {"data1",       a.data1},
                {"data2",       a.data2},
                {"extra",       a.extra},
                {"label",       a.label},
                {"tsMs",        a.tsMs}
            };
            arr.push_back(std::move(item));
        }
        return arr;
        };

    j["undo"] = dumpStack(undoStack);
    j["redo"] = dumpStack(redoStack);
    j["meta"] = {
        {"maxEntries",        maxEntries},
        {"savepointUndoSize", savepointUndoSize},
        {"savepointRedoSize", savepointRedoSize}
    };

    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to write history dump: " << path << "\n";
        return;
    }
    f << j.dump(4);
}