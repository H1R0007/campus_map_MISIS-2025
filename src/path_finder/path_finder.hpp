#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../map/Node.hpp"
#include "../map/GraphManager.hpp"

// Структура элемента для очереди с приоритетом (для A*)
struct Point {
    std::string id;   // id узла
    float f_score;    // оценка f = g + h

    // нужно для std::priority_queue (она сортирует по "меньше")
    bool operator<(const Point& other) const {
        return f_score > other.f_score; // инверсия, так как priority_queue — max-heap
    }
};

struct PathFinderOptions {
    bool allowStairs = true;
    bool allowLift = true;
    bool allowBridge = true;
    bool allowDoor = true;
};

// Главная функция поиска пути
std::vector<std::string> find_shortest_path(
    const std::string& start_id,
    const std::string& end_id,
    const GraphManager& graphManager,
    const PathFinderOptions& options = {}
);