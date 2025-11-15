#include "GraphManager.hpp"
#include "MetaLoader.hpp"
#include "../utils/safe_stoi.hpp"
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string snapshotNodeToJson(const Node& n, const std::string& graphKey) {
    json j;
    j["graph"] = graphKey;
    j["id"] = n.id;
    j["x"] = n.x;
    j["y"] = n.y;
    j["floor"] = n.floor;
    j["building"] = n.building;
    j["isPortal"] = n.isPortal;
    j["neighbors"] = n.neighbors;
    return j.dump();
}

static std::string extractGraphKeyFromExtra(const std::string& extra, const std::string& fallback) {
    if (extra.empty()) return fallback;
    try {
        auto j = json::parse(extra);
        if (j.contains("graph")) return j["graph"].get<std::string>();
        if (j.contains("node") && j["node"].contains("graph"))
            return j["node"]["graph"].get<std::string>();
    }
    catch (...) {}
    return fallback;
}

Graph* GraphManager_getGraphByKey(Graph& campusGraph,
    std::unordered_map<std::string, Graph>& graphs,
    const std::string& key)
{
    if (key == "__campus") return &campusGraph;
    auto it = graphs.find(key);
    if (it != graphs.end()) return &it->second;
    return nullptr;
}

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
    if (activeKey == key) return;

    // History: SwitchView (только если не в undo/redo и не первая установка)
    if (!performingUndoRedo && !activeKey.empty()) {
        json j;
        j["from"] = activeKey;
        j["to"] = key;
        HistoryAction a;
        a.type = ActionType::SwitchView;
        a.data1 = activeKey;
        a.data2 = key;
        a.extra = j.dump();
        history.push(a);
    }

    activeKey = key;
}

std::string GraphManager::addNode(int x, int y, int floor) {
    Graph* g = (activeKey == "__campus") ? &campusGraph : &graphs[activeKey];
    std::string newId = g->addNode(x, y, floor);

    if (!performingUndoRedo) {
        const Node* n = g->getNode(newId);
        if (n) {
            json j;
            j["graph"] = activeKey;
            j["id"] = n->id;
            j["x"] = n->x;
            j["y"] = n->y;
            j["floor"] = n->floor;
            j["building"] = n->building;
            j["isPortal"] = n->isPortal;
            j["neighbors"] = n->neighbors;

            HistoryAction a;
            a.type = ActionType::AddNode;
            a.data1 = newId;
            a.extra = j.dump();
            history.push(a);
        }
    }
    return newId;
}

void GraphManager::removeNodeById(const std::string& id) {
    Graph* g = (activeKey == "__campus") ? &campusGraph : &graphs[activeKey];

    if (!performingUndoRedo) {
        const Node* n = g->getNode(id);
        if (n) {
            json j;
            j["graph"] = activeKey;
            j["id"] = n->id;
            j["x"] = n->x;
            j["y"] = n->y;
            j["floor"] = n->floor;
            j["building"] = n->building;
            j["isPortal"] = n->isPortal;
            j["neighbors"] = n->neighbors;

            HistoryAction a;
            a.type = ActionType::RemoveNode;
            a.data1 = id;
            a.extra = j.dump();
            history.push(a);
        }
    }

    g->removeNodeById(id);
}


void GraphManager::addNeighbor(const std::string& a, const std::string& b) {
    Graph* g = (activeKey == "__campus") ? &campusGraph : &graphs[activeKey];

    g->addNeighbor(a, b);

    if (!performingUndoRedo) {
        json j; j["graph"] = activeKey;
        HistoryAction ha;
        ha.type = ActionType::AddNeighbor;
        ha.data1 = a;
        ha.data2 = b;
        ha.extra = j.dump();
        history.push(ha);
    }
}

void GraphManager::removeNeighbor(const std::string& a, const std::string& b) {
    Graph* g = (activeKey == "__campus") ? &campusGraph : &graphs[activeKey];

    g->removeNeighbor(a, b);

    if (!performingUndoRedo) {
        json j; j["graph"] = activeKey;
        HistoryAction ha;
        ha.type = ActionType::RemoveNeighbor;
        ha.data1 = a;
        ha.data2 = b;
        ha.extra = j.dump();
        history.push(ha);
    }
}

void GraphManager::addTransition(const Transition& t) {
    transitions.addTransition(t);

    if (!performingUndoRedo) {
        json j;
        j["type"] = transitionTypeToString(t.type);
        HistoryAction ha;
        ha.type = ActionType::AddTransition;
        ha.data1 = t.fromNode;
        ha.data2 = t.toNode;
        ha.extra = j.dump();
        history.push(ha);
    }
}

void GraphManager::removeTransition(const std::string& from, const std::string& to) {
    // Пытаемся узнать тип до удаления
    std::optional<TransitionType> tp = getTransitionType(from, to);
    transitions.removeTransition(from, to);

    if (!performingUndoRedo) {
        json j;
        if (tp.has_value()) j["type"] = transitionTypeToString(*tp);
        else                j["type"] = "unknown";
        HistoryAction ha;
        ha.type = ActionType::RemoveTransition;
        ha.data1 = from;
        ha.data2 = to;
        ha.extra = j.dump();
        history.push(ha);
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


void GraphManager::undoGlobal() {
    auto actOpt = history.undo();
    if (!actOpt.has_value()) return;
    const HistoryAction& act = *actOpt;

    performingUndoRedo = true;
    const std::string graphKey = extractGraphKeyFromExtra(act.extra, activeKey);
    Graph* g = GraphManager_getGraphByKey(campusGraph, graphs, graphKey);
    if (!g) g = GraphManager_getGraphByKey(campusGraph, graphs, activeKey); // форс

    switch (act.type) {
    case ActionType::AddNode:
        // Отменяем добавление узла — корректно удаляем с очисткой обратных ссылок
        if (g) g->removeNodeById(act.data1);
        break;

    case ActionType::RemoveNode:
        // Отменяем удаление — восстановим узел и обратные ссылки
        if (!act.extra.empty()) {
            try {
                auto j = json::parse(act.extra);
                std::string gk = j.value("graph", activeKey);
                Graph* gg = GraphManager_getGraphByKey(campusGraph, graphs, gk);
                if (!gg) gg = g;
                if (gg) {
                    Node n;
                    n.id = j.value("id", "");
                    n.x = j.value("x", 0);
                    n.y = j.value("y", 0);
                    n.floor = j.value("floor", 0);
                    n.building = j.value("building", "CAMPUS");
                    n.isPortal = j.value("isPortal", false);
                    n.neighbors = j.value("neighbors", std::vector<std::string>{});
                    if (!n.id.empty()) {
                        auto& nodes = gg->getNodesMutable();
                        nodes[n.id] = n;

                        // Восстановим обратные ссылки у соседей
                        for (const std::string& nbId : n.neighbors) {
                            auto itNb = nodes.find(nbId);
                            if (itNb != nodes.end()) {
                                auto& nbList = itNb->second.neighbors;
                                if (std::find(nbList.begin(), nbList.end(), n.id) == nbList.end())
                                    nbList.push_back(n.id);
                            }
                        }
                    }
                }
            }
            catch (...) {}
        }
        break;

    case ActionType::AddNeighbor:
        if (g) g->removeNeighbor(act.data1, act.data2); // односторонний вызов, Graph делает двусторонне
        break;

    case ActionType::RemoveNeighbor:
        if (g) g->addNeighbor(act.data1, act.data2);    // односторонний вызов, Graph делает двусторонне
        break;

    case ActionType::AddTransition:
        transitions.removeTransition(act.data1, act.data2);
        break;

    case ActionType::RemoveTransition:
        try {
            auto jt = json::parse(act.extra);
            Transition t{ act.data1, act.data2, parseTransitionType(jt.value("type", "unknown")) };
            transitions.addTransition(t);
        }
        catch (...) {}
        break;

    case ActionType::EditAlias:
        // Вне GraphManager — пропускаем
        break;

    case ActionType::SwitchView:
        try {
            auto j = json::parse(act.extra);
            std::string from = j.value("from", activeKey);
            setActiveGraph(from);
        }
        catch (...) {}
        break;

    default: break;
    }

    performingUndoRedo = false;
}

void GraphManager::redoGlobal() {
    auto actOpt = history.redo();
    if (!actOpt.has_value()) return;
    const HistoryAction& act = *actOpt;

    performingUndoRedo = true;
    const std::string graphKey = extractGraphKeyFromExtra(act.extra, activeKey);
    Graph* g = GraphManager_getGraphByKey(campusGraph, graphs, graphKey);
    if (!g) g = GraphManager_getGraphByKey(campusGraph, graphs, activeKey);

    switch (act.type) {
    case ActionType::AddNode:
        // Повторяем добавление узла — восстановим снимок
        restoreNodeFromJson(act.extra);
        break;

    case ActionType::RemoveNode:
        // Повторяем удаление — корректно удаляем с очисткой обратных ссылок
        if (g) g->removeNodeById(act.data1);
        break;

    case ActionType::AddNeighbor:
        if (g) g->addNeighbor(act.data1, act.data2); // односторонний вызов
        break;

    case ActionType::RemoveNeighbor:
        if (g) g->removeNeighbor(act.data1, act.data2); // односторонний вызов
        break;

    case ActionType::AddTransition: {
        try {
            auto jt = json::parse(act.extra);
            Transition t{ act.data1, act.data2, parseTransitionType(jt.value("type", "unknown")) };
            transitions.addTransition(t);
        }
        catch (...) {}
        break;
    }

    case ActionType::RemoveTransition:
        transitions.removeTransition(act.data1, act.data2);
        break;

    case ActionType::EditAlias:
        break;

    case ActionType::SwitchView:
        try {
            auto j = json::parse(act.extra);
            std::string to = j.value("to", activeKey);
            setActiveGraph(to);
        }
        catch (...) {}
        break;

    default: break;
    }

    performingUndoRedo = false;
}

void GraphManager::restoreNodeFromJson(const std::string& jsonData) {
    if (jsonData.empty()) return;
    try {
        auto j = json::parse(jsonData);
        std::string graphKey = j.value("graph", activeKey);

        Graph* g = GraphManager_getGraphByKey(campusGraph, graphs, graphKey);
        if (!g) g = GraphManager_getGraphByKey(campusGraph, graphs, activeKey);
        if (!g) return;

        Node n;
        n.id = j.value("id", "");
        n.x = j.value("x", 0);
        n.y = j.value("y", 0);
        n.floor = j.value("floor", 0);
        n.building = j.value("building", "CAMPUS");
        n.isPortal = j.value("isPortal", false);
        n.neighbors = j.value("neighbors", std::vector<std::string>{});

        if (n.id.empty()) return;

        auto& nodes = g->getNodesMutable();
        nodes[n.id] = n; // вставка/замена

        // Восстановим обратные ссылки у соседей
        for (const std::string& nbId : n.neighbors) {
            auto itNb = nodes.find(nbId);
            if (itNb != nodes.end()) {
                auto& nbList = itNb->second.neighbors;
                if (std::find(nbList.begin(), nbList.end(), n.id) == nbList.end())
                    nbList.push_back(n.id);
            }
        }

    }
    catch (...) {
        // ignore malformed json
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