#include "path_finder.hpp"
#include <queue>
#include <unordered_map>
#include <limits>
#include <cmath>
#include <algorithm>
#include <iostream>

// === Heuristic (private) ===
// Approximates distance between nodes for A* priority.
// Uses Euclidean distance in (x,y), plus penalty for floor difference.
static float heuristic(const Node& a, const Node& b) {
    float dx = float(a.x - b.x);
    float dy = float(a.y - b.y);
    int df = std::abs(a.floor - b.floor);

    const float FLOOR_COST = 100.0f; // "FLOOR_COST" defines virtual z-distance per floor
    return std::sqrt(dx * dx + dy * dy + (df * FLOOR_COST) * (df * FLOOR_COST));
}

// === Edge cost (private) ===
// Computes traversal cost between two nodes (direct graph edge).
// Uses TransitionType if applicable — allows banning stairs, etc.
static float edge_cost(const Node& a, const Node& b, const GraphManager& graphManager,
    const PathFinderOptions& options)
{
    // Check if this edge represents a transition (stairs, door, lift, bridge etc.)
    auto trType = graphManager.getTransitionType(a.id, b.id);
    if (trType) {
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

    // Edge across floors without transition = unrealistic, mark as very costly.
    if (a.floor == b.floor) {
        float dx = float(a.x - b.x), dy = float(a.y - b.y);
        return std::sqrt(dx * dx + dy * dy);
    }

    // переход без transitions (странно, но пусть очень дорогой)
    return 1000.0f;
}


// === Main A* Implementation ===
std::vector<std::string> find_shortest_path(
    const std::string& start_id,
    const std::string& end_id,
    const GraphManager& graphManager,
    const PathFinderOptions& options
) {
    const int MAX_VISITS = 10000;
    int visits = 0;
    // Guard: ensure start/end exist
    if (graphManager.getNode(start_id) == nullptr ||
        graphManager.getNode(end_id) == nullptr) {
        return {};
    }

    std::unordered_map<std::string, float> g_score;     // cost from start
    std::unordered_map<std::string, std::string> came_from; // predecessor chain

    g_score[start_id] = 0.0f;

    // Priority queue ordered by f = g + h
    std::priority_queue<Point> open_set;
    open_set.push({ start_id, heuristic(*graphManager.getNode(start_id),
                                        *graphManager.getNode(end_id)) });

    while (!open_set.empty()) {
        if (++visits > MAX_VISITS) {
            std::cerr << "[PathFinder] Warning: iteration limit hit (possible loop)\n";
            break;
        }
        auto current = open_set.top().id;
        open_set.pop();

        // Reached goal → reconstruct path
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

    return {}; // No path found
}