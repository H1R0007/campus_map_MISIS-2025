#pragma once
#include <string>
#include <vector>
#include "BuildingMeta.hpp"

// === MetaLoader ===
// Responsible for loading metadata for campus and buildings.
// Unlike Graph, this does not load nodes, just meta-structures.
class MetaLoader {
public:
    // Load list of building IDs from campus meta file.
    // Example: from "campus/meta.json".
    static std::vector<std::string> loadCampusBuildings(const std::string& path);

    // Load metadata (id, name, floors) of a specific building.
    // Example: from "Building_X/meta.json".
    static BuildingMeta loadBuildingMeta(const std::string& path);
};