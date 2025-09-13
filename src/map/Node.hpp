#pragma once
#include <string>
#include <vector>

struct Node {
    std::string id;                       // уникальный идентификатор узла
    int x = 0;
    int y = 0;
    int floor = 0;
    std::vector<std::string> neighbors;   // id соседей
};