#include "MetaLoader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

std::vector<std::string> MetaLoader::loadCampusBuildings(const std::string& path) {
    std::vector<std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open campus meta: " << path << "\n";
        return result;
    }
    json j;
    file >> j;
    if (!j.contains("buildings")) return result;

    for (auto& b : j["buildings"]) {
        result.push_back(b.value("id", ""));
    }
    return result;
}

BuildingMeta MetaLoader::loadBuildingMeta(const std::string& path) {
    BuildingMeta m;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open building meta: " << path << "\n";
        return m;
    }

    json j;
    file >> j;
    m.id = j.value("building_id", "");
    m.name = j.value("name", "");

    if (j.contains("floors")) {
        for (auto& f : j["floors"]) {
            FloorMeta fm;
            fm.floor = f.value("floor", 0);
            fm.mapPath = f.value("map", "");
            fm.graphPath = f.value("graph", "");
            m.floors.push_back(fm);
        }
    }
    return m;
}