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
        std::cerr << "Failed to open transitions: " << path << "\n";
        return false;
    }

    json j;
    try {
        file >> j;
    }
    catch (const std::exception& e) {
        std::cerr << "TransitionManager: parse error " << e.what() << "\n";
        return false;
    }

    if (!j.contains("transitions") || !j["transitions"].is_array()) {
        std::cerr << "TransitionManager: invalid structure (no transitions)\n";
        return false;
    }

    // Ensure uniqueness — avoid duplicates like A<->B twice
    transitions.clear();
    std::set<std::pair<std::string, std::string>> uniq;

    for (auto& t : j["transitions"]) {

        if (!t.contains("from") || !t.contains("to")) {
            std::cerr << "TransitionManager: missing from/to\n";
            continue;
        }

        if (!t["from"].contains("node") || !t["to"].contains("node")) {
            std::cerr << "TransitionManager: invalid transition format\n";
            continue;
        }

        Transition tr;
        tr.fromNode = t["from"]["node"];
        tr.toNode = t["to"]["node"];

        std::string typeStr = t.value("transition_type", "unknown");
        tr.type = parseTransitionType(typeStr);

        auto key = std::minmax(tr.fromNode, tr.toNode);
        if (uniq.count(key)) {
            std::cerr << "TransitionManager: duplicate transition " << tr.fromNode
                << " <-> " << tr.toNode << "\n";
            continue;
        }
        uniq.insert(key);

        transitions.push_back(tr);
    }
    std::cout << "Loaded " << transitions.size() << " transitions\n";
    return true;
}

bool TransitionManager::saveToJson(const std::string& path) const {
    json j;
    for (auto& tr : transitions) {
        j["transitions"].push_back({
            {"from", { {"node", tr.fromNode} }},
            {"to",   { {"node", tr.toNode} }},
            {"transition_type", transitionTypeToString(tr.type)}
            });
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;
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