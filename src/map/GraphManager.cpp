#include "GraphManager.hpp"
#include "MetaLoader.hpp"
#include "../utils/safe_stoi.hpp"
#include <iostream>
#include <nlohmann/json.hpp>

// === Data Loading ===
bool GraphManager::loadCampus(const std::string& path) {
    return campusGraph.loadFromJson(path);
}

bool GraphManager::loadCampusMeta(const std::string& path) {
    auto buildings = MetaLoader::loadCampusBuildings(path);
    for (auto& b : buildings) {
        std::string metaPath = "assets/buildings/" + b + "/meta.json";
        loadBuildingWithMeta(b, metaPath);
    }
    return true;
}

bool GraphManager::loadBuildingWithMeta(const std::string& buildingId, const std::string& metaPath) {
    BuildingMeta bm = MetaLoader::loadBuildingMeta(metaPath);
    buildingMetas[buildingId] = bm;
    for (auto& f : bm.floors) {
        loadBuildingFloor(buildingId + "_floor_" + std::to_string(f.floor),
            "assets/buildings/" + buildingId + "/" + f.graphPath);
    }
    return true;
}

bool GraphManager::loadBuildingFloor(const std::string& key, const std::string& path) {
    Graph g;
    if (!g.loadFromJson(path)) return false;
    graphs[key] = std::move(g);
    graphPaths[key] = path;   // сохраняем путь для этого графа
    return true;
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

// === Metadata ===
const BuildingMeta* GraphManager::getBuildingMeta(const std::string& id) const {
    auto it = buildingMetas.find(id);
    if (it != buildingMetas.end()) return &it->second;
    return nullptr;
}


// === Navigation Helpers ===
const Node* GraphManager::getNode(const std::string& id) const {
    if (const Node* n = campusGraph.getNode(id)) return n;

    // Ищем в активном графе, если он есть
    if (graphs.count(activeKey)) {
        if (const Node* n = graphs.at(activeKey).getNode(id)) {
            return n;
        }
    }

    for (auto& [k, g] : graphs) {
        if (const Node* n = g.getNode(id)) return n;
    }
    return nullptr;
}

std::vector<std::string> GraphManager::getNeighbors(const std::string& id) const {
    const Node* node = getNode(id);
    if (!node) {
        return {};
    }

    std::vector<std::string> res = node->neighbors; 

    auto transitionNeighbors = transitions.getLinkedNodes(id);
    res.insert(res.end(), transitionNeighbors.begin(), transitionNeighbors.end());

    return res;
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


// === Editing (active graph) ===
void GraphManager::setActiveGraph(const std::string& key) {
    if (key == "__campus") {
        activeKey = "__campus"; // special mask for campus
    }
    else {
        activeKey = key;
    }
}

std::string GraphManager::addNode(int x, int y, int floor) {
    std::string id;
    std::string buildingId = "CAMPUS";
    int nodeFloor = floor;

    // Определяем, куда добавляем (кампус или этаж)
    if (activeKey == "__campus") {
        // --- CAMPUS ---
        id = "CAMPUS_0_NODE_" + std::to_string(globalNextId++);
        campusGraph.loadNode(id, x, y, 0, {}); // campus-floor всегда 0
    }
    else if (graphs.count(activeKey)) {
        // --- BUILDING FLOOR ---
        auto pos = activeKey.find("_floor_");
        if (pos != std::string::npos) {
            buildingId = activeKey.substr(0, pos);             // "Building_A"
            try {
                nodeFloor = std::stoi(activeKey.substr(pos + 7)); // 1, 2, ...
            }
            catch (...) { nodeFloor = floor; }
        }

        // Префикс: A / B / C и т.п.
        std::string prefix = buildingId.substr(buildingId.find("_") + 1);
        // prefix = "A"

        id = prefix + "_" + std::to_string(nodeFloor) + "_NODE_" + std::to_string(globalNextId++);

        graphs[activeKey].loadNode(id, x, y, nodeFloor, {});
    }

    // Устанавливаем дополнительные поля ( building / floor )
    if (Node* n = const_cast<Node*>(getNode(id))) {
        n->building = buildingId;
        n->floor = nodeFloor;
    }

    // === Запись истории (если это не Undo/Redo) ===
    if (!performingUndoRedo) {
        nlohmann::json snap = {
            {"id", id},
            {"x", x},
            {"y", y},
            {"building", buildingId},
            {"floor", nodeFloor},
            {"isPortal", false},
            {"neighbors", nlohmann::json::array()}
        };
        history.push({ ActionType::AddNode, id, "", snap.dump() });
    }

    std::cout << "Added node: " << id
        << " (" << x << "," << y << ", floor " << nodeFloor << ") in "
        << buildingId << std::endl;

    return id;
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

    auto transitions_to_check = transitions.getTransitions();
    for (const auto& tr : transitions_to_check) {
        if (tr.fromNode == id || tr.toNode == id) {
            // This call will trigger its own history push if not in undo/redo mode
            removeTransition(tr.fromNode, tr.toNode);
        }
    }

    if (activeKey == "__campus") {
        campusGraph.removeNodeById(id);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].removeNodeById(id);
    }

    if (!performingUndoRedo) {
        history.push({ ActionType::RemoveNode, id, "", snap.dump() });
    }
}


void GraphManager::addNeighbor(const std::string& a, const std::string& b) {
    if (activeKey == "__campus") {
        campusGraph.addNeighbor(a, b);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].addNeighbor(a, b);
    }

    if (!performingUndoRedo) {
        history.push({ ActionType::AddNeighbor, a, b, "" });
    }
}

void GraphManager::removeNeighbor(const std::string& a, const std::string& b) {
    if (activeKey == "__campus") {
        campusGraph.removeNeighbor(a, b);
    }
    else if (graphs.count(activeKey)) {
        graphs[activeKey].removeNeighbor(a, b);
    }

    if (!performingUndoRedo) {
        history.push({ ActionType::RemoveNeighbor, a, b, "" });
    }
}

void GraphManager::addTransition(const Transition& t) {
    transitions.addTransition(t);

    if (Node* n = const_cast<Node*>(getNode(t.fromNode))) {
        n->isPortal = true;
    }
    else std::cerr << "Transition target missing\n";

    if (Node* n = const_cast<Node*>(getNode(t.toNode))) {
        n->isPortal = true;
    }
    else std::cerr << "Transition target missing\n";

    if (!performingUndoRedo) {
        history.push({ ActionType::AddTransition, t.fromNode, t.toNode, transitionTypeToString(t.type) });
    }
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

    auto stillLinked = [&](const std::string& nodeId) {
        for (auto& tr : transitions.getTransitions()) {
            if (tr.fromNode == nodeId || tr.toNode == nodeId)
                return true;
        }
        return false;
    };

    if (Node* n = const_cast<Node*>(getNode(from))) {
        if (!stillLinked(from)) n->isPortal = false;
    }
    if (Node* n = const_cast<Node*>(getNode(to))) {
        if (!stillLinked(to)) n->isPortal = false;
    }

    if (!performingUndoRedo) {
        history.push({ ActionType::RemoveTransition, from, to, transitionTypeToString(type) });
    }
}


// === Save/load ===
void GraphManager::saveActive() {
#ifdef __EMSCRIPTEN__
    std::cout << "[GraphManager] saveActive() ignored in WebAssembly environment\n";
    return;
#endif

    if (activeKey == "__campus") {
        campusGraph.saveToJson("assets/campus/graph.json");
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

void GraphManager::saveTransitions(const std::string& path) {
    transitions.saveToJson(path);
}


// === Node Accessors ===
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


// === Undo/Redo ===
void GraphManager::undoGlobal() {
    auto act = history.undo();
    if (!act) return;

    performingUndoRedo = true;
    try {
        switch (act->type) {
        case ActionType::AddNode:
            removeNodeById(act->data1);
            break;
        case ActionType::RemoveNode:
            restoreNodeFromJson(act->extra);
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
    catch (...) {
        std::cerr << "[GraphManager] Undo/Redo failed\n";
    }
    performingUndoRedo = false;
}

void GraphManager::redoGlobal() {
    auto act = history.redo();
    if (!act) return;

    try {
        performingUndoRedo = true;
        switch (act->type) {
        case ActionType::AddNode:
            restoreNodeFromJson(act->extra);
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
    catch (...) {
        std::cerr << "[GraphManager] Undo/Redo failed\n";
    }

    performingUndoRedo = false;
}

// === Private Helpers ===
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

// Чтобы корректно создавать новые точки по линиям
void GraphManager::recalculateGlobalNextId() {
    int maxId = 1;

    auto extractIdNum = [](const std::string& id) -> int {
        // безопасно выдергиваем последний числовой суффикс, например "A_1_NODE_27" → 27
        size_t pos = id.find_last_of('_');
        if (pos != std::string::npos && pos + 1 < id.size()) {
            return safeStoi(id.substr(pos + 1), 0);
        }
        return 0;
        };

    // пройтись по всем графам, включая кампус
    for (auto& [_, node] : campusGraph.getNodes()) {
        maxId = std::max(maxId, extractIdNum(node.id));
    }
    for (auto& [_, nodeSet] : graphs) {
        for (auto& [_, node] : nodeSet.getNodes()) {
            maxId = std::max(maxId, extractIdNum(node.id));
        }
    }

    globalNextId = maxId + 1;
    std::cout << "[GraphManager] globalNextId recalculated -> " << globalNextId << std::endl;
}