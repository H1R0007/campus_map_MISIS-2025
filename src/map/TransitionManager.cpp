#include "TransitionManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <set>

using json = nlohmann::json;


// === I/O ===
bool TransitionManager::loadFromJson(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open transitions file: " << path << "\n";
        return false;
    }

    nlohmann::json j;
    try {
        file >> j;
    }
    catch (const std::exception& e) {
        std::cerr << "[TransitionManager] Parse error in " << path
            << ": " << e.what() << "\n";
        return false;
    }

    if (!j.contains("transitions") || !j["transitions"].is_array()) {
        std::cerr << "[TransitionManager] Invalid structure (" << path << ")\n";
        return false;
    }

    transitions.clear();
    std::set<std::pair<std::string, std::string>> uniq;

    for (auto& t : j["transitions"]) {
        // Базовые проверки
        if (!t.contains("from") || !t.contains("to")
            || !t["from"].contains("node") || !t["to"].contains("node")) {
            std::cerr << "[TransitionManager] Skipping malformed entry in " << path << "\n";
            continue;
        }

        std::string a = t["from"]["node"].get<std::string>();
        std::string b = t["to"]["node"].get<std::string>();
        std::string typeStr = t.value("transition_type", "unknown");

        auto key = std::minmax(a, b);
        if (uniq.count(key)) {
            std::cerr << "[TransitionManager] Duplicate transition " << a << " <-> " << b << "\n";
            continue;
        }
        uniq.insert(key);

        Transition tr{ a, b, parseTransitionType(typeStr) };
        transitions.push_back(tr);
    }

    std::cout << "[TransitionManager] Loaded " << transitions.size()
        << " transitions from " << path << "\n";
    return !transitions.empty();
}

bool TransitionManager::saveToJson(const std::string& path) const {
    nlohmann::json j;
    j["transitions"] = nlohmann::json::array();

    for (auto& tr : transitions) {
        j["transitions"].push_back({
            {"from", { {"node", tr.fromNode} }},
            {"to",   { {"node", tr.toNode} }},
            {"transition_type", transitionTypeToString(tr.type)}
            });
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to save transitions: " << path << "\n";
        return false;
    }
    file << j.dump(4);
    return true;
}


// === Access ===
std::vector<std::string> TransitionManager::getLinkedNodes(const std::string& nodeId) const {
    std::vector<std::string> results;
    for (auto& tr : transitions) {
        if (tr.fromNode == nodeId) results.push_back(tr.toNode);
        if (tr.toNode == nodeId) results.push_back(tr.fromNode);
    }
    return results;
}


// === Editing ===
void TransitionManager::addTransition(const Transition& t) {
    if (t.fromNode == t.toNode) {
        std::cerr << "[TransitionManager] Ignored self-loop " << t.fromNode << "\n";
        return;
    }
    for (auto& existing : transitions) {
        if ((existing.fromNode == t.fromNode && existing.toNode == t.toNode) ||
            (existing.fromNode == t.toNode && existing.toNode == t.fromNode)) {
            std::cerr << "Transition already exists: "
                << t.fromNode << " <-> " << t.toNode << "\n";
            return;
        }
    }
    transitions.push_back(t);
}

void TransitionManager::removeTransition(const std::string& fromNode, const std::string& toNode) {
    transitions.erase(
        std::remove_if(transitions.begin(), transitions.end(),
            [&](const Transition& tr) {
                return (tr.fromNode == fromNode && tr.toNode == toNode) ||
                    (tr.fromNode == toNode && tr.toNode == fromNode);
            }),
        transitions.end()
    );
}