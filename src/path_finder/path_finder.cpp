#include <path_finder/path_finder.hpp>


// Штраф при смене этажа + евклидово пространство
static float floor_count(int from_id, int to_id) {
    const Node& from = nods_database[from_id];
    const Node& to = nods_database[to_id];

    float dx = from.x - to.x;
    float dy = from.y - to.y;
    float floorPenalty = (from.floor != to.floor) ? 50.0f : 0.0f;  // Штраф за переход между этажами

    return std::sqrt(dx * dx + dy * dy) + floorPenalty;
}

std::vector<int> find_shortest_path(int start_id, int end_id) {
    // Минимальное расстояние до точки "g"
    std::unordered_map<int, float> g_length;
    // Откуда пришли (для восстановления пути)
    std::unordered_map<int, int> came_from;
    // Приоритетная очередь: {f = g + h, point_id}
    std::priority_queue<Point, std::vector<Point>, std::greater<>> open_set;

    for (const auto& [id, _] : nodes_database) {
        g_length[id] = std::numeric_limits<float>::infinity();
    }
    g_length[start_id] = 0.0f;
    open_set.push({ start_id, floor_count(start_id, end_id) });

    while (!open_set.empty()) {
        Point current = open_set.top();
        open_set.pop();

        if (current.id == end_id) {
            // Восстановление пути
            std::vector<int> path;
            for (int at = end_id; at != start_id; at = came_from[at]) {
                path.push_back(at);
            }
            path.push_back(start_id);
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (const auto& [neighbor_id, distance] : nodes_database[current.id].neighbors) {
            float tentative_length = g_length[current.id] + distance;
            if (tentative_length < g_length[neighbor_id]) {
                came_from[neighbor_id] = current.id;
                g_length[neighbor_id] = tentative_length;
                float final_length = tentative_length + floor_count(neighbor_id, end_id);
                open_set.push({ neighbor_id, final_length });
            }
        }
    }

    return {};  // Путь не найден
}