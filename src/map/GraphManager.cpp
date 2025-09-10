#include "GraphManager.hpp"
#include "MetaLoader.hpp"
#include <iostream>
#include <nlohmann/json.hpp>

bool GraphManager::loadCampus(const std::string& path) {
    return campusGraph.loadFromJson(path);
}

bool GraphManager::loadBuildingFloor(const std::string& key, const std::string& path) {
    Graph g;
    if (!g.loadFromJson(path)) return false;
    graphs[key] = std::move(g);
    graphPaths[key] = path;   // сохраняем путь для этого графа
    return true;
}

bool GraphManager::loadCampusMeta(const std::string& path) {
    auto buildings = MetaLoader::loadCampusBuildings(path);
    for (auto& b : buildings) {
        // сразу загружаем building meta если нужно, или по требованию
        std::string metaPath = "assets/buildings/" + b + "/meta.json";
        loadBuildingWithMeta(b, metaPath);
    }
    return true;
}

bool GraphManager::loadBuildingWithMeta(const std::string& buildingId, const std::string& metaPath) {
    BuildingMeta bm = MetaLoader::loadBuildingMeta(metaPath);
    buildingMetas[buildingId] = bm;
    // сразу загружаем графы этажей
    for (auto& f : bm.floors) {
        loadBuildingFloor(buildingId + "_floor_" + std::to_string(f.floor),
            "assets/buildings/" + buildingId + "/" + f.graphPath);
    }
    return true;
}

const BuildingMeta* GraphManager::getBuildingMeta(const std::string& id) const {
    auto it = buildingMetas.find(id);
    if (it != buildingMetas.end()) return &it->second;
    return nullptr;
}

bool GraphManager::loadTransitions(const std::string& path) {
    if (!transitions.loadFromJson(path)) return false;

    // simple sanity check
    for (auto& tr : transitions.getTransitions()) {
        if (!getNode(tr.fromNode) || !getNode(tr.toNode)) {
            std::cerr << "Warning: transition " << tr.fromNode
                << " -> " << tr.toNode
                << " refers to missing node!\n";
        }
    }
    return true;
}

const Node* GraphManager::getNode(const std::string& id) const {
    if (const Node* n = campusGraph.getNode(id)) return n;
    for (auto& [k, g] : graphs) {
        if (const Node* n = g.getNode(id)) return n;
    }
    return nullptr;
}

std::vector<std::string> GraphManager::getNeighbors(const std::string& id) const {
    std::vector<std::string> res;

    if (const Node* n = campusGraph.getNode(id))
        res.insert(res.end(), n->neighbors.begin(), n->neighbors.end());

    for (auto& [k, g] : graphs) {
        if (const Node* n = g.getNode(id)) {
            res.insert(res.end(), n->neighbors.begin(), n->neighbors.end());
            break;
        }
    }

    // плюс переходы
    auto extra = transitions.getLinkedNodes(id);
    res.insert(res.end(), extra.begin(), extra.end());

    return res;
}

// ==================== РЕДАКТОРСКИЕ ФУНКЦИИ ====================

void GraphManager::setActiveGraph(const std::string& key) {
    if (key == "__campus") {
        activeKey = "__campus"; // спец‑маска для кампуса
    }
    else {
        activeKey = key;
    }
}

void GraphManager::addNode(int x, int y, int floor) {
    Node node;
    node.id = "temp_" + std::to_string(rand()); // генератор id пока заглушка
    node.x = x;
    node.y = y;
    node.floor = floor;
    node.building = (activeKey == "__campus") ? "CAMPUS" : activeKey;
    node.isPortal = false;

    if (activeKey == "__campus") {
        campusGraph.loadNode(node.id, node.x, node.y, node.floor, {});
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].loadNode(node.id, node.x, node.y, node.floor, {});
    }

    nlohmann::json snap = {
        {"id", node.id},
        {"x", node.x},
        {"y", node.y},
        {"floor", node.floor},
        {"building", node.building},
        {"isPortal", node.isPortal},
        {"neighbors", node.neighbors}
    };

    history.push({ ActionType::AddNode, node.id, "", snap.dump() });
}

void GraphManager::removeNodeById(const std::string& id) {
    const Node* victim = getNode(id);
    if (!victim) return;

    nlohmann::json snap = {
        {"id", victim->id},
        {"x", victim->x},
        {"y", victim->y},
        {"floor", victim->floor},
        {"building", victim->building},
        {"isPortal", victim->isPortal},
        {"neighbors", victim->neighbors}
    };

    if (activeKey == "__campus") {
        campusGraph.removeNodeById(id, false);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].removeNodeById(id, false);
    }

    history.push({ ActionType::RemoveNode, id, "", snap.dump() });
}

void GraphManager::addNeighbor(const std::string& a, const std::string& b) {
    if (activeKey == "__campus") {
        campusGraph.addNeighbor(a, b);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].addNeighbor(a, b);
    }

    history.push({ ActionType::AddNeighbor, a, b, "" });
}

void GraphManager::removeNeighbor(const std::string& a, const std::string& b) {
    if (activeKey == "__campus") {
        campusGraph.removeNeighbor(a, b);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].removeNeighbor(a, b);
    }

    history.push({ ActionType::RemoveNeighbor, a, b, "" });
}

void GraphManager::undoGlobal() {
    auto act = history.undo();
    if (!act) return;

    switch (act->type) {
    case ActionType::AddNode:
        removeNodeById(act->data1);
        break;
    case ActionType::RemoveNode:
        restoreNodeFromJson(act->extra);
        break;
        break;
    case ActionType::AddNeighbor:
        removeNeighbor(act->data1, act->data2);
        break;
    case ActionType::RemoveNeighbor:
        addNeighbor(act->data1, act->data2);
        break;
    case ActionType::AddTransition:
        removeTransition(act->data1, act->data2);
        break;
    case ActionType::RemoveTransition:
        addTransition({ act->data1, act->data2, parseTransitionType(act->extra) });
        break;
    default:
        break;
    }
}

void GraphManager::redoGlobal() {
    auto act = history.redo();
    if (!act) return;

    // делаем обратное "undo" (т.е. повторяем действие)
    switch (act->type) {
    case ActionType::AddNode:
        restoreNodeFromJson(act->extra); // повторяем добавление по снапшоту
        break;
    case ActionType::RemoveNode:
        removeNodeById(act->data1);
        break;
    case ActionType::AddNeighbor:
        addNeighbor(act->data1, act->data2);
        break;
    case ActionType::RemoveNeighbor:
        removeNeighbor(act->data1, act->data2);
        break;
    case ActionType::AddTransition:
        addTransition({ act->data1, act->data2, parseTransitionType(act->extra) });
        break;
    case ActionType::RemoveTransition:
        removeTransition(act->data1, act->data2);
        break;
    default:
        break;
    }
}

void GraphManager::saveActive() {
    if (activeKey == "__campus") {
        campusGraph.saveToJson("assets/campus/graph.json"); // фиксированный путь
    }
    else if (graphs.count(activeKey)) {
        if (graphPaths.count(activeKey)) {
            graphs[activeKey].saveToJson(graphPaths[activeKey]);
        }
        else {
            graphs[activeKey].saveToJson(activeKey + "_nodes.json"); // fallback
        }
    }
}

void GraphManager::addTransition(const Transition& t) {
    transitions.addTransition(t);
    history.push({ ActionType::AddTransition, t.fromNode, t.toNode, transitionTypeToString(t.type) });
}

void GraphManager::removeTransition(const std::string& from, const std::string& to) {
    TransitionType type = TransitionType::Unknown;
    for (auto& tr : transitions.getTransitions()) {
        if ((tr.fromNode == from && tr.toNode == to) ||
            (tr.fromNode == to && tr.toNode == from)) {
            type = tr.type;
        }
    }

    transitions.removeTransition(from, to);
    history.push({ ActionType::RemoveTransition, from, to, transitionTypeToString(type) });
}

void GraphManager::saveTransitions(const std::string& path) {
    transitions.saveToJson(path);
}

const std::unordered_map<std::string, Node>& GraphManager::getActiveNodes() const {
    static std::unordered_map<std::string, Node> empty;
    if (graphs.count(activeKey))
        return graphs.at(activeKey).getNodes();
    return empty;
}

const std::unordered_map<std::string, Node>& GraphManager::getCampusNodes() const {
    return campusGraph.getNodes();
}

std::unordered_map<std::string, Node>& GraphManager::getActiveNodesMutable() {
    static std::unordered_map<std::string, Node> dummy;
    if (graphs.count(activeKey))
        return graphs.at(activeKey).getNodesMutable();
    return dummy;
}

void GraphManager::restoreNodeFromJson(const std::string& jsonData) {
    if (jsonData.empty()) return;

    try {
        nlohmann::json snap = nlohmann::json::parse(jsonData);

        Node node;
        node.id = snap.value("id", "");
        node.x = snap.value("x", 0);
        node.y = snap.value("y", 0);
        node.floor = snap.value("floor", 0);
        node.building = snap.value("building", "");
        node.isPortal = snap.value("isPortal", false);

        if (snap.contains("neighbors")) {
            for (auto& nb : snap["neighbors"]) {
                node.neighbors.push_back(nb.get<std::string>());
            }
        }

        if (node.building == "CAMPUS") {
            campusGraph.loadNode(node.id, node.x, node.y, node.floor, node.neighbors);
        }
        else {
            std::string key = node.building + "_floor_" + std::to_string(node.floor);
            if (graphs.count(key)) {
                graphs[key].loadNode(node.id, node.x, node.y, node.floor, node.neighbors);
            }
            else {
                std::cerr << "restoreNodeFromJson: target graph not loaded for " << key << "\n";
            }
        }

        std::cout << "Restored node: " << node.id << "\n";
    }
    catch (std::exception& e) {
        std::cerr << "restoreNodeFromJson parse error: " << e.what() << "\n";
    }
}

std::optional<TransitionType> GraphManager::getTransitionType(const std::string& a, const std::string& b) const {
    for (auto& tr : transitions.getTransitions()) {
        if ((tr.fromNode == a && tr.toNode == b) ||
            (tr.fromNode == b && tr.toNode == a)) {
            return tr.type;
        }
    }
    return std::nullopt;
}