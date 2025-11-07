#include "MetaLoader.hpp"
#include "../utils/safe_stoi.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// === Public API ===
std::vector<std::string> MetaLoader::loadCampusBuildings(const std::string& path) {
    std::vector<std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open campus meta: " << path << "\n";
        return result;  // return empty list on error
    }
    json j;
    file >> j;
    if (!j.contains("buildings")) return result;

    // Iterate through campus["buildings"], collect their IDs
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
        return m;  // empty meta in case of error
    }

    json j;
    file >> j;
    m.id = j.value("building_id", "");
    m.name = j.value("name", "");

    if (j.contains("floors") && j["floors"].is_array()) {
        for (auto& f : j["floors"]) {
            FloorMeta fm;

            // --- floor ---
            if (f.contains("floor")) {
                if (f["floor"].is_number_integer())
                    fm.floor = f["floor"].get<int>();
                else if (f["floor"].is_string()) {
                    std::string val = f["floor"].get<std::string>();
                    fm.floor = safeStoi(val, 0);
                }
                else
                    fm.floor = 0;
            }

            // --- map & graph paths ---
            fm.mapPath = f.value("map", "");
            fm.graphPath = f.value("graph", "");

            // === Проверка на корректность ===
            if (fm.mapPath.empty() || fm.graphPath.empty()) {
                std::cerr << "[MetaLoader] Warning: floor entry missing map or graph path ("
                    << path << ")\n";
            }

            m.floors.push_back(fm);
        }
    }
    return m;
}