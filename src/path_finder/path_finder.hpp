#pragma once
#include <string>
#include <vector>
#include <string>
#include <unordered_map>
#include "../map/Node.hpp"

// Структура элемента для очереди с приоритетом (для A*)
struct Point {
    std::string id;   // id узла
    float f_score;    // оценка f = g + h

    // нужно для std::priority_queue (она сортирует по "меньше")
    bool operator<(const Point& other) const {
        return f_score > other.f_score; // инверсия, так как priority_queue — max-heap
    }
};

// Главная функция поиска пути
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