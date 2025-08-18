#pragma once
#include <string>
#include "../map/Graph.hpp"

class JsonLoader {
public:
    // Загрузить граф из JSON файла
    static Graph loadGraph(const std::string& filename);
};