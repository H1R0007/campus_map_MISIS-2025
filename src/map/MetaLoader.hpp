#pragma once
#include <string>
#include <vector>
#include "BuildingMeta.hpp"

class MetaLoader {
public:
    static std::vector<std::string> loadCampusBuildings(const std::string& path);
    static BuildingMeta loadBuildingMeta(const std::string& path);
};