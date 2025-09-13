#pragma once
#include <string>
#include <vector>

struct Node {
    std::string id;                       // уникальный идентификатор узла
    int x = 0;
    int y = 0;
    int floor = 0;
    std::string building;             // ID корпуса (например "B")
    bool isPortal = false;            // этот узел портал (вход/лестница/лифт)
    std::vector<std::string> neighbors;   // id соседей
};