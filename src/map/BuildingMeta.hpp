#pragma once
#include <string>
#include <vector>

// === FloorMeta ===
// Metadata for a single floor of a building.
// Contains floor index, path to map image, and path to graph json.
struct FloorMeta {
    int floor = 0;
    std::string mapPath;
    std::string graphPath;
};

// === BuildingMeta ===
// Metadata for an entire building: its id, display name,
// and a list of floors with associated resources.
struct BuildingMeta {
    std::string id;               // internal building id (e.g. "Building_A")
    std::string name;             // human-friendly name (e.g. "Corpus_A")
    std::vector<FloorMeta> floors;

    bool isValid() const {
        return !id.empty() && !floors.empty();
    }
};