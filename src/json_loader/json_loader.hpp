#pragma once
#include <string>
#include "../map/Graph.hpp"

// === JsonLoader ===
// Responsible for loading graph definition from a JSON file.
// Transforms JSON nodes into in-memory Graph structure.
class JsonLoader {
public:

    // Load graph from a JSON file located at 'filename'.
    // Throws std::runtime_error if file cannot be opened.
    static Graph loadGraph(const std::string& filename);
};