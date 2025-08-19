#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "../map/Node.hpp"


struct Point {
    std::string id;
    float f_score;

    bool operator<(const Point& other) const {
        return f_score > other.f_score; // оператор перевёрнут специально для min-кучи
    }
};

std::vector<std::string> find_shortest_path(
    const std::string& start_id,
    const std::string& end_id,
    const std::unordered_map<std::string, Node>& nodes_database
);

static float heuristic(const Node& a, const Node& b) {
    float dx = float(a.x - b.x);
    float dy = float(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}