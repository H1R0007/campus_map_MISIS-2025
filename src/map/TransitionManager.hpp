#pragma once
#include <string>
#include <vector>
#include "TransitionType.hpp"

// === Transition ===
// Data structure for a portal (connection) between two nodes.
struct Transition {
    std::string fromNode;
    std::string toNode;
    TransitionType type;  // "door", "stairs", "lift", "bridge"
};


// === TransitionManager ===
// Stores and manages all cross-graph transitions.
// Supports load/save from JSON and runtime manipulation.
class TransitionManager {
public:
    TransitionManager() = default;

    // === I/O ===
    bool loadFromJson(const std::string& path);
    bool saveToJson(const std::string& path) const;

    // === Access ===
    const std::vector<Transition>& getTransitions() const { return transitions; }

    // For given node -> get all directly linked nodes across transitions.
    std::vector<std::string> getLinkedNodes(const std::string& nodeId) const;

    // === Editing ===
    void addTransition(const Transition& t);
    void removeTransition(const std::string& fromNode, const std::string& toNode);

private:
    std::vector<Transition> transitions;
};