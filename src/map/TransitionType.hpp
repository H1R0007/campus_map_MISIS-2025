#pragma once
#include <string>

// === TransitionType ===
// Represents type of portal/connection between graph nodes.
enum class TransitionType {
    Unknown = 0, Door, Stairs, Lift, Bridge
};
// === Utility functions ===
// String <-> Enum conversion
inline TransitionType parseTransitionType(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
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