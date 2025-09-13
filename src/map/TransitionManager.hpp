#pragma once
#include <string>
#include <vector>
#include "TransitionType.hpp"

struct Transition {
    std::string fromNode;
    std::string toNode;
    TransitionType type;  // "door", "stairs", "lift", "bridge"
};

class TransitionManager {
public:
    bool loadFromJson(const std::string& path);
    const std::vector<Transition>& getTransitions() const { return transitions; }

    // ѕолучить всех соседей дл€ узла через переходы
    std::vector<std::string> getLinkedNodes(const std::string& nodeId) const;

    void addTransition(const Transition& t);
    void removeTransition(const std::string& fromNode, const std::string& toNode);
    bool saveToJson(const std::string& path) const;

private:
    std::vector<Transition> transitions;
};