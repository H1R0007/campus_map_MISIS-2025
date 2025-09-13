#pragma once
#include <string>

enum class TransitionType {
    Door,
    Stairs,
    Lift,
    Bridge,
    Unknown
};

inline TransitionType parseTransitionType(const std::string& s) {
    if (s == "door") return TransitionType::Door;
    if (s == "stairs") return TransitionType::Stairs;
    if (s == "lift") return TransitionType::Lift;
    if (s == "bridge") return TransitionType::Bridge;
    return TransitionType::Unknown;
}

inline std::string transitionTypeToString(TransitionType t) {
    switch (t) {
    case TransitionType::Door: return "door";
    case TransitionType::Stairs: return "stairs";
    case TransitionType::Lift: return "lift";
    case TransitionType::Bridge: return "bridge";
    default: return "unknown";
    }
}