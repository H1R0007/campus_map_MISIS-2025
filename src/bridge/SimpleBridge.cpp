#include "SimpleBridge.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string SimpleBridge::getVersion() {
    return "1.0.0";
}

std::string SimpleBridge::getBuildings() {
    json buildings = {
        {"buildings", {
            {{"id", "Building_A"}, {"name", "Корпус А"}},
            {{"id", "Building_B"}, {"name", "Корпус Б"}},
            {{"id", "Building_C"}, {"name", "Корпус В"}}
        }}
    };
    return buildings.dump();
}

std::string SimpleBridge::calculateRoute(const std::string& from, const std::string& to) {
    json route = {
        {"from", from},
        {"to", to},
        {"path", {from, "A_1_COR_1", to}},
        {"distance", 150}
    };
    return route.dump();
}