#include "path_finder.hpp"
#include <queue>
#include <unordered_map>
#include <limits>
#include <cmath>
#include <algorithm>

// Эвристика A*: "примерная стоимость" от узла к цели
// Учитываем этаж как Z-координату с фиксированной ценой за разницу этажей
static float heuristic(const Node& a, const Node& b) {
    float dx = float(a.x - b.x);
    float dy = float(a.y - b.y);
    int df = std::abs(a.floor - b.floor);

    const float FLOOR_COST = 100.0f; // "цена" перехода между этажами
    return std::sqrt(dx * dx + dy * dy + (df * FLOOR_COST) * (df * FLOOR_COST));
}

// Реальная стоимость ребра (от узла к соседу)
// Тут различаем обычное перемещение по этажу и переход между этажами
static float edge_cost(const Node& a, const Node& b) {
    if (a.floor == b.floor) {
        float dx = float(a.x - b.x);
        float dy = float(a.y - b.y);
        return std::sqrt(dx * dx + dy * dy);
    }
    else {
        // фиксированная цена подъёма/спуска
        return 100.0f;
    }
}

std::vector<std::string> find_shortest_path(
    const std::string& start_id,
    const std::string& end_id,
    const std::unordered_map<std::string, Node>& nodes_database
) {

    if (nodes_database.find(start_id) == nodes_database.end() ||
        nodes_database.find(end_id) == nodes_database.end()) {
        return {};
    }

    std::unordered_map<std::string, float> g_score;
    std::unordered_map<std::string, std::string> came_from;

    // Инициализируем оценки расстояний
    for (auto& [id, _] : nodes_database) {
        g_score[id] = std::numeric_limits<float>::infinity();
    }
    g_score[start_id] = 0.0f;

    // Очередь открытых узлов (по f_score)
    std::priority_queue<Point> open_set;
    open_set.push({ start_id, heuristic(nodes_database.at(start_id), nodes_database.at(end_id)) });

    while (!open_set.empty()) {
        auto current = open_set.top().id;
        open_set.pop();

        if (current == end_id) {
            // Восстановим путь
            std::vector<std::string> path;
            for (std::string at = end_id; !at.empty(); at = came_from.count(at) ? came_from[at] : "") {
                path.push_back(at);
                if (at == start_id) break;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (auto& neighbor_id : nodes_database.at(current).neighbors) {

            if (nodes_database.find(neighbor_id) == nodes_database.end()) {
                continue; // если сосед "битый"
            }

            float tentative_g = g_score[current] +
                edge_cost(nodes_database.at(current), nodes_database.at(neighbor_id));

            if (tentative_g < g_score[neighbor_id]) {
                came_from[neighbor_id] = current;
                g_score[neighbor_id] = tentative_g;

                float f = tentative_g +
                    heuristic(nodes_database.at(neighbor_id), nodes_database.at(end_id));

                open_set.push({ neighbor_id, f });
            }
        }
    }

    return {}; // путь не найден
}