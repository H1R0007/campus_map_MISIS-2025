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

static float edge_cost(const Node& a, const Node& b, const GraphManager& graphManager,
    const PathFinderOptions& options)
{
    // проверяем: это transition или обычное ребро?
    auto trType = graphManager.getTransitionType(a.id, b.id);
    if (trType) {
        // фильтрация
        switch (*trType) {
        case TransitionType::Stairs:
            if (!options.allowStairs) return std::numeric_limits<float>::infinity();
            return 100.0f;
        case TransitionType::Lift:
            if (!options.allowLift) return std::numeric_limits<float>::infinity();
            return 10.0f;
        case TransitionType::Bridge:
            if (!options.allowBridge) return std::numeric_limits<float>::infinity();
            return 20.0f;
        case TransitionType::Door:
            if (!options.allowDoor) return std::numeric_limits<float>::infinity();
            return 1.0f;
        default:
            return 50.0f;
        }
    }

    // обычный сосед в том же графе
    if (a.floor == b.floor) {
        float dx = float(a.x - b.x), dy = float(a.y - b.y);
        return std::sqrt(dx * dx + dy * dy);
    }

    // переход без transitions (странно, но пусть очень дорогой)
    return 1000.0f;
}

std::vector<std::string> find_shortest_path(
    const std::string& start_id,
    const std::string& end_id,
    const GraphManager& graphManager,
    const PathFinderOptions& options
) {
    if (graphManager.getNode(start_id) == nullptr ||
        graphManager.getNode(end_id) == nullptr) {
        return {};
    }

    std::unordered_map<std::string, float> g_score;
    std::unordered_map<std::string, std::string> came_from;

    g_score[start_id] = 0.0f;

    std::priority_queue<Point> open_set;
    open_set.push({ start_id, heuristic(*graphManager.getNode(start_id),
                                        *graphManager.getNode(end_id)) });

    while (!open_set.empty()) {
        auto current = open_set.top().id;
        open_set.pop();

        if (current == end_id) {
            std::vector<std::string> path;
            for (std::string at = end_id; !at.empty(); at = came_from.count(at) ? came_from[at] : "") {
                path.push_back(at);
                if (at == start_id) break;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        const Node* currentNode = graphManager.getNode(current);
        if (!currentNode) continue;

        for (auto& neighbor_id : graphManager.getNeighbors(current)) {
            const Node* nbNode = graphManager.getNode(neighbor_id);
            if (!nbNode) continue;

            float cost = edge_cost(*currentNode, *nbNode, graphManager, options);
            if (cost == std::numeric_limits<float>::infinity()) {
                continue; // этот переход запрещён
            }

            float tentative_g = g_score[current] + cost;

            if (g_score.find(neighbor_id) == g_score.end() ||
                tentative_g < g_score[neighbor_id]) {
                came_from[neighbor_id] = current;
                g_score[neighbor_id] = tentative_g;

                float f = tentative_g +
                    heuristic(*nbNode, *graphManager.getNode(end_id));

                open_set.push({ neighbor_id, f });
            }
        }
    }

    return {}; // путь не найден
}