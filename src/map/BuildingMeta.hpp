#pragma once
#include <string>
#include <vector>

struct FloorMeta {
    int floor;
    std::string mapPath;
    std::string graphPath;
};

struct BuildingMeta {
    std::string id;
    std::string name;
    std::vector<FloorMeta> floors;
};