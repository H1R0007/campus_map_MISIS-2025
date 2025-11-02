#include "InputHandler.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../config.hpp"
#include "../map/BuildingMeta.hpp"
#include "../map/TransitionType.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

InputHandler::InputHandler(MapViewer & viewer, Camera & camera, GraphManager & graphManager)
    : mapViewer(viewer), camera(camera), graphManager(graphManager) {
}

void InputHandler::handleEvent(const SDL_Event& event) {
    // 1. First, handle UI-specific input, which might consume the event.
    // This includes clicks on text fields and typing.
    if (mapViewer.inputActive) {
        if (event.type == SDL_TEXTINPUT || (event.type == SDL_KEYDOWN && (
            event.key.keysym.sym == SDLK_BACKSPACE ||
            event.key.keysym.sym == SDLK_TAB ||
            event.key.keysym.sym == SDLK_UP ||
            event.key.keysym.sym == SDLK_DOWN ||
            event.key.keysym.sym == SDLK_RETURN ||
            event.key.keysym.sym == SDLK_KP_ENTER
            ))) {
            processUIInput(event);
            return; // UI input consumes the event, stop further processing.
        }
    }

    // 2. If not consumed by UI, process general application events.
    switch (event.type) {
    case SDL_KEYDOWN:
        handleKeyDown(event);
        break;
    case SDL_MOUSEBUTTONDOWN:
        handleMouseButtonDown(event);
        break;
    case SDL_MOUSEMOTION:
        handleMouseMotion(event);
        break;
    case SDL_MOUSEWHEEL:
        handleMouseWheel(event);
        break;
    default:
        break;
    }
}

// Handles input ONLY when a UI text field is active
void InputHandler::processUIInput(const SDL_Event& event) {
    if (event.type == SDL_TEXTINPUT) {
        handleTextInput(event);
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_BACKSPACE:
            if (mapViewer.editingFrom && !mapViewer.inputFrom.empty()) mapViewer.inputFrom.pop_back();
            else if (!mapViewer.editingFrom && !mapViewer.inputTo.empty()) mapViewer.inputTo.pop_back();
            mapViewer.selectedSuggestionIndex = -1;
            break;
        case SDLK_TAB:
            mapViewer.editingFrom = !mapViewer.editingFrom;
            mapViewer.selectedSuggestionIndex = -1;
            break;
        case SDLK_UP:
            if (!mapViewer.currentSuggestions.empty()) {
                mapViewer.selectedSuggestionIndex--;
                if (mapViewer.selectedSuggestionIndex < 0) mapViewer.selectedSuggestionIndex = mapViewer.currentSuggestions.size() - 1;
            }
            break;
        case SDLK_DOWN:
            if (!mapViewer.currentSuggestions.empty()) {
                mapViewer.selectedSuggestionIndex = (mapViewer.selectedSuggestionIndex + 1) % mapViewer.currentSuggestions.size();
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (mapViewer.selectedSuggestionIndex != -1 && mapViewer.selectedSuggestionIndex < mapViewer.currentSuggestions.size()) {
                if (mapViewer.editingFrom) mapViewer.inputFrom = mapViewer.currentSuggestions[mapViewer.selectedSuggestionIndex];
                else mapViewer.inputTo = mapViewer.currentSuggestions[mapViewer.selectedSuggestionIndex];
                mapViewer.selectedSuggestionIndex = -1;
            }
            else {
                mapViewer.buildPathFromAliases(mapViewer.inputFrom, mapViewer.inputTo);
            }
            break;
        default:
            // Let other keys (like arrows for map navigation) pass through if needed,
            // but for now, we consume them to avoid side effects.
            break;
        }
    }
}

void InputHandler::handleTextInput(const SDL_Event& event) {
    if (mapViewer.editingFrom) {
        mapViewer.inputFrom += event.text.text;
    }
    else {
        mapViewer.inputTo += event.text.text;
    }
    mapViewer.selectedSuggestionIndex = -1; // Reset suggestion on new input
}

void InputHandler::handleKeyDown(const SDL_Event& event) {
    // --- User Options (always available) ---
    switch (event.key.keysym.sym) {
    case SDLK_8: mapViewer.userAllowStairs = !mapViewer.userAllowStairs; std::cout << "Option: allowStairs = " << (mapViewer.userAllowStairs ? "ON" : "OFF") << "\n"; return;
    case SDLK_9: mapViewer.userAllowLift = !mapViewer.userAllowLift; std::cout << "Option: allowLift = " << (mapViewer.userAllowLift ? "ON" : "OFF") << "\n"; return;
    case SDLK_0: mapViewer.userAllowBridge = !mapViewer.userAllowBridge; std::cout << "Option: allowBridge = " << (mapViewer.userAllowBridge ? "ON" : "OFF") << "\n"; return;
    }

    // --- Map Navigation (always available) ---
    switch (event.key.keysym.sym) {
    case SDLK_TAB:
        if (mapViewer.currentView == ViewMode::Campus) {
            if (!mapViewer.currentBuilding.empty()) mapViewer.switchToFloor(mapViewer.currentBuilding, mapViewer.currentFloor);
        }
        else { mapViewer.switchViewToCampus(); }
        return;
    case SDLK_RIGHT:
        if (mapViewer.currentView == ViewMode::BuildingFloor) {
            if (mapViewer.currentBuilding == "Building_A") mapViewer.switchToFloor("Building_B", 1);
            else if (mapViewer.currentBuilding == "Building_B") mapViewer.switchToFloor("Building_C", 1);
            else mapViewer.switchToFloor("Building_A", 1);
        }
        else if (mapViewer.currentView == ViewMode::Campus) { mapViewer.switchToFloor("Building_A", 1); }
        return;
    case SDLK_LEFT:
        if (mapViewer.currentView == ViewMode::BuildingFloor) {
            if (mapViewer.currentBuilding == "Building_C") mapViewer.switchToFloor("Building_B", 1);
            else if (mapViewer.currentBuilding == "Building_B") mapViewer.switchToFloor("Building_A", 1);
            else mapViewer.switchToFloor("Building_C", 1);
        }
        else if (mapViewer.currentView == ViewMode::Campus) { mapViewer.switchToFloor("Building_C", 1); }
        return;
    case SDLK_UP: {
        const BuildingMeta* bm = graphManager.getBuildingMeta(mapViewer.currentBuilding);
        if (bm) { for (int i = 0; i < (int)bm->floors.size(); i++) { if (bm->floors[i].floor == mapViewer.currentFloor && i + 1 < (int)bm->floors.size()) { mapViewer.switchToFloor(mapViewer.currentBuilding, bm->floors[i + 1].floor); break; } } }
        return;
    }
    case SDLK_DOWN: {
        const BuildingMeta* bm = graphManager.getBuildingMeta(mapViewer.currentBuilding);
        if (bm) { for (int i = 0; i < (int)bm->floors.size(); i++) { if (bm->floors[i].floor == mapViewer.currentFloor && i > 0) { mapViewer.switchToFloor(mapViewer.currentBuilding, bm->floors[i - 1].floor); break; } } }
        return;
    }
    }

    // --- Developer-only features ---
    // Wrapped in #ifndef to completely disable for Emscripten builds
#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        processDevKeyInput(event);
    }
#endif
}

void InputHandler::handleMouseButtonDown(const SDL_Event& event) {
    SDL_Point clickScreen{ event.button.x, event.button.y };

    // --- UI Focus Check (LMB only) ---
    // This logic is now here to keep all mouse button logic together.
    if (event.button.button == SDL_BUTTON_LEFT) {
        if (SDL_PointInRect(&clickScreen, &mapViewer.fromFieldRect)) { mapViewer.inputActive = true; mapViewer.editingFrom = true; return; }
        if (SDL_PointInRect(&clickScreen, &mapViewer.toFieldRect)) { mapViewer.inputActive = true; mapViewer.editingFrom = false; return; }
        mapViewer.inputActive = false; // Click elsewhere clears focus.
    }

    // --- Developer-only features ---
    // Wrapped in #ifndef to completely disable for Emscripten builds
#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        processDevMouseInput(event);
    }
#endif
}


void InputHandler::handleMouseMotion(const SDL_Event& event) {
    mapViewer.debugMouseWorld = camera.screenToWorld({ event.motion.x, event.motion.y });

    // Panning with LMB drag (only if not in UI input mode)
    if (event.motion.state & SDL_BUTTON_LMASK && !mapViewer.inputActive) {
        camera.move(-event.motion.xrel, -event.motion.yrel);
    }

    // --- Developer-only features ---
#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        mapViewer.hoveredNode = nullptr;
        std::string targetId = mapViewer.findNodeUnderCursor({ event.motion.x, event.motion.y });
        if (!targetId.empty()) {
            mapViewer.hoveredNode = graphManager.getNode(targetId);
        }

        // Update LineTool preview
        if (mapViewer.lineToolActive && mapViewer.lineToolFirstPointSet) {
            mapViewer.lineToolEnd = mapViewer.debugMouseWorld;
        }
    }
#endif
}

void InputHandler::handleMouseWheel(const SDL_Event& event) {
    SDL_Point mousePos;
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    float zoomFactor = (event.wheel.y > 0) ? 1.1f : 0.9f;

    const Uint8* state = SDL_GetKeyboardState(nullptr);
    if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) {
        camera.zoom(zoomFactor, ZoomMode::Mouse, mousePos);
    }
    else {
        camera.zoom(zoomFactor, ZoomMode::Center);
    }
}

// --- Developer-only input processing methods ---

#ifndef __EMSCRIPTEN__
void InputHandler::processDevKeyInput(const SDL_Event& event) {
    const bool isCtrl = SDL_GetModState() & KMOD_CTRL;

    // Undo/Redo/Save
    if (isCtrl && event.key.keysym.sym == SDLK_z) { graphManager.undoGlobal(); return; }
    if (isCtrl && event.key.keysym.sym == SDLK_y) { graphManager.redoGlobal(); return; }
    if (isCtrl && event.key.keysym.sym == SDLK_s) {
        if (SDL_GetModState() & KMOD_SHIFT) { graphManager.saveTransitions(Config::TRANSITIONS_PATH); }
        else { mapViewer.lastSaveTick = SDL_GetTicks(); graphManager.saveActive(); }
        return;
    }

    switch (event.key.keysym.sym) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if (mapViewer.neighborMode && !mapViewer.activeNodeId.empty()) {
            for (const auto& nb : mapViewer.pendingNeighbors) { graphManager.addNeighbor(mapViewer.activeNodeId, nb); }
            mapViewer.neighborMode = false; mapViewer.activeNodeId.clear(); mapViewer.pendingNeighbors.clear();
        }
        // Line tool finalization from console input
        else if (mapViewer.lineToolReady) {
            std::cout << "[LineTool] Creating nodes with default parameters." << std::endl;
            mapViewer.createNodeLine(mapViewer.lineToolStart, mapViewer.lineToolAngleDeg, mapViewer.lineToolCount, mapViewer.lineToolStep);
            mapViewer.lineToolReady = false; // —брасываем флаг
        }
        break;
    case SDLK_ESCAPE:
        if (mapViewer.neighborMode) { mapViewer.neighborMode = false; mapViewer.activeNodeId.clear(); mapViewer.pendingNeighbors.clear(); }
        if (!mapViewer.portalStartNode.empty()) { mapViewer.portalStartNode.clear(); }
        if (mapViewer.lineToolActive) { mapViewer.lineToolActive = false; mapViewer.lineToolFirstPointSet = false; }
        break;
    case SDLK_l:
        if (isCtrl) {
            mapViewer.lineToolActive = !mapViewer.lineToolActive;
            mapViewer.lineToolFirstPointSet = false;
            mapViewer.lineToolReady = false;
            if (mapViewer.lineToolActive) {
                std::cout << "[LineTool] Activated. Left-click to set start point." << std::endl;
            }
            else {
                std::cout << "[LineTool] Deactivated." << std::endl;
            }
        }
        break;
    }
}

void InputHandler::processDevMouseInput(const SDL_Event& event) {
    SDL_Point clickScreen{ event.button.x, event.button.y };
    SDL_Point clickWorld = camera.screenToWorld(clickScreen);
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    std::string target = mapViewer.findNodeUnderCursor(clickScreen);

    // --- Left Mouse Button: Path selection & Line Tool ---
    if (event.button.button == SDL_BUTTON_LEFT) {
        // Line Tool has priority if active
        if (mapViewer.lineToolActive) {
            if (!mapViewer.lineToolFirstPointSet) {
                mapViewer.lineToolStart = clickWorld;
                mapViewer.lineToolEnd = clickWorld;
                mapViewer.lineToolFirstPointSet = true;
                std::cout << "[LineTool] Start point set at (" << clickWorld.x << ", " << clickWorld.y << "). Select an end point." << std::endl;
            }
            else {
                // Second click: finalize the line's geometry and prepare for node creation
                mapViewer.lineToolEnd = clickWorld;
                mapViewer.lineToolFirstPointSet = false;
                mapViewer.lineToolActive = false;

                float dx = static_cast<float>(mapViewer.lineToolEnd.x - mapViewer.lineToolStart.x);
                float dy = static_cast<float>(mapViewer.lineToolEnd.y - mapViewer.lineToolStart.y);

                // Calculate default angle and distance from the drawn line
                float autoAngle = std::atan2(dy, dx) * 180.0f / static_cast<float>(M_PI);
                mapViewer.lineToolDistance = std::sqrt(dx * dx + dy * dy);

#ifndef __EMSCRIPTEN__
// For native builds, we can ask for precise parameters via console.

                std::cout << "---------------------------------" << std::endl;
                std::cout << "[LineTool] Define line parameters:" << std::endl;

                // --- 1. Get Node Count ---
                std::cout << "  - Node Count (default: " << mapViewer.lineToolCount << "): ";
                std::string line;
                std::getline(std::cin, line);
                if (!line.empty()) {
                    try {
                        mapViewer.lineToolCount = std::max(2, std::stoi(line)); // Must be at least 2
                    }
                    catch (const std::exception&) {
                        std::cout << "    Invalid input. Using default count." << std::endl;
                    }
                }

                // --- 2. Get Angle ---
                std::cout << "  - Angle in degrees (default from mouse: " << autoAngle << "): ";
                std::getline(std::cin, line);
                if (!line.empty()) {
                    try {
                        mapViewer.lineToolAngleDeg = std::stof(line);
                    }
                    catch (const std::exception&) {
                        std::cout << "    Invalid input. Using angle from mouse." << std::endl;
                        mapViewer.lineToolAngleDeg = autoAngle;
                    }
                }
                else {
                    // User pressed Enter, use the angle from the mouse drawing
                    mapViewer.lineToolAngleDeg = autoAngle;
                }

                std::cout << "---------------------------------" << std::endl;

#else
// For Emscripten builds, just use the auto-calculated angle and default count
                mapViewer.lineToolAngleDeg = autoAngle;
                // lineToolCount remains its default value
#endif

                mapViewer.lineToolReady = true;
                std::cout << "[LineTool] Ready to create " << mapViewer.lineToolCount << " nodes at "
                    << mapViewer.lineToolAngleDeg << " degrees." << std::endl;
                std::cout << "[LineTool] Press Enter to confirm." << std::endl;
            }
            return;
        }

        // Path selection logic
        if (!target.empty()) {
            if (!mapViewer.startNodeId.empty() && !mapViewer.endNodeId.empty()) {
                mapViewer.startNodeId.clear();
                mapViewer.endNodeId.clear();
                mapViewer.currentPath.clear();
                std::cout << "[Path] Selection reset." << std::endl;
            }
            if (mapViewer.startNodeId.empty()) {
                mapViewer.startNodeId = target;
                std::cout << "[Path] Start node selected: " << target << std::endl;
            }
            else {
                mapViewer.endNodeId = target;
                mapViewer.buildPathFromAliases(mapViewer.startNodeId, mapViewer.endNodeId);
            }
        }
        return;
    }

    // --- Right Mouse Button: Node/Edge creation and modification ---
    if (event.button.button == SDL_BUTTON_RIGHT) {
        // --- Actions with Modifier Keys (have priority) ---
        if (keys[SDL_SCANCODE_LALT]) { // Alt + RMB = Enter/Exit Neighbor Mode
            if (!target.empty()) {
                mapViewer.neighborMode = true;
                mapViewer.activeNodeId = target;
                mapViewer.pendingNeighbors.clear();
                std::cout << "[Neighbor Mode] Activated for node: " << target << ". Right-click on other nodes to stage them." << std::endl;
            }
            return;
        }
        if (keys[SDL_SCANCODE_P]) { // P + RMB = Portal Mode
            if (!target.empty()) {
                if (mapViewer.portalStartNode.empty()) {
                    mapViewer.portalStartNode = target;
                    std::cout << "[Portal Mode] Start node selected: " << target << ". Press P+RMB on another node to create a portal." << std::endl;
                }
                else if (mapViewer.portalStartNode != target) {
                    graphManager.addTransition({ mapViewer.portalStartNode, target, TransitionType::Door });
                    std::cout << "[Portal Mode] Portal created: " << mapViewer.portalStartNode << " <-> " << target << std::endl;
                    mapViewer.portalStartNode.clear();
                }
            }
            return;
        }
        if (keys[SDL_SCANCODE_LCTRL]) { // Ctrl + RMB = Inspector
            if (!target.empty()) {
                mapViewer.inspectorNodeId = (mapViewer.inspectorNodeId == target) ? "" : target;
                if (!mapViewer.inspectorNodeId.empty()) {
                    std::cout << "[Inspector] Showing info for node: " << target << std::endl;
                }
                else {
                    std::cout << "[Inspector] Closed." << std::endl;
                }
            }
            return;
        }
        if (keys[SDL_SCANCODE_LSHIFT]) { // Shift + RMB = Delete
            if (!target.empty()) {
                if (mapViewer.neighborMode && mapViewer.activeNodeId != target) {
                    // In neighbor mode, we want to remove the connection.
                    // First, check if it's a pending (yellow) connection.
                    auto& pending = mapViewer.pendingNeighbors;
                    auto it = std::find(pending.begin(), pending.end(), target);
                    if (it != pending.end()) {
                        pending.erase(it);
                        std::cout << "[Neighbor Mode] Unstaged pending neighbor: " << target << std::endl;
                    }
                    else {
                        // If not pending, it must be an existing (gray) connection. Remove it.
                        std::cout << "[Neighbor Mode] Removing existing edge between " << mapViewer.activeNodeId << " and " << target << std::endl;
                        graphManager.removeNeighbor(mapViewer.activeNodeId, target);
                    }
                }
                else if (!mapViewer.neighborMode) {
                    // Outside neighbor mode, it deletes the node itself.
                    std::cout << "[Node] Deleting node: " << target << std::endl;
                    graphManager.removeNodeById(target);
                }
            }
            return;
        }

        // --- Actions without Modifier Keys (context-dependent) ---
        if (mapViewer.neighborMode) {
            // If in neighbor mode, a simple RMB stages a neighbor for connection
            if (!target.empty() && target != mapViewer.activeNodeId) {
                // Check if it's already a neighbor to avoid duplicates
                auto& existingNeighbors = graphManager.getNode(mapViewer.activeNodeId)->neighbors;
                if (std::find(existingNeighbors.begin(), existingNeighbors.end(), target) != existingNeighbors.end()) {
                    std::cout << "[Neighbor Mode] Node " << target << " is already a neighbor." << std::endl;
                }
                else if (std::find(mapViewer.pendingNeighbors.begin(), mapViewer.pendingNeighbors.end(), target) == mapViewer.pendingNeighbors.end()) {
                    mapViewer.pendingNeighbors.push_back(target);
                    std::cout << "[Neighbor Mode] Staged neighbor: " << target << ". Press Enter to confirm all." << std::endl;
                }
            }
        }
        else {
            // If not in any mode, a simple RMB creates a new node
            std::string newNodeId = graphManager.addNode(clickWorld.x, clickWorld.y, mapViewer.currentFloor);
            std::cout << "[Node] Created new node: " << newNodeId << " at (" << clickWorld.x << ", " << clickWorld.y << ")" << std::endl;
        }
    }
}
#endif // __EMSCRIPTEN__