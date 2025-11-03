#include "UIManager.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../config.hpp"
#include <iostream>
#include <string>

// Helper to use ImGui::InputText with std::string
namespace ImGui {
    static int InputTextCallback(ImGuiInputTextCallbackData* data) {
        std::string* str = (std::string*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    }
    bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0) {
        flags |= ImGuiInputTextFlags_CallbackResize;
        return ImGui::InputTextWithHint(label, hint, str->data(), str->capacity() + 1, flags, InputTextCallback, (void*)str);
    }
}

// --- Конструктор ---
UIManager::UIManager() {}

// --- render() ---
void UIManager::render(MapViewer& mapViewer) {
    drawSearchWindow(mapViewer);

#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        // Теперь мы вызываем отрисовку нового окна
        drawDevInfoWindow(mapViewer);
        drawNodeInspector(mapViewer);
    }
#endif
}

void UIManager::drawSearchWindow(MapViewer& mapViewer) {
    ImGui::SetNextWindowSize(ImVec2(300, 0));
    ImGui::Begin("Route Search", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // --- Данные ---
    std::string& fromStr = mapViewer.getInputFrom();
    std::string& toStr = mapViewer.getInputTo();

    // --- Поля ввода ---
    if (ImGui::InputTextWithHint("##From", "From...", &fromStr)) {
        mapViewer.setEditingFrom(true);
        mapViewer.updateSuggestions();
    }
    if (ImGui::IsItemActive()) {
        mapViewer.setEditingFrom(true);
    }

    if (ImGui::InputTextWithHint("##To", "To...", &toStr)) {
        mapViewer.setEditingFrom(false);
        mapViewer.updateSuggestions();
    }
    if (ImGui::IsItemActive()) {
        mapViewer.setEditingFrom(false);
    }

    // --- Кнопка ---
    ImGui::Spacing();
    if (ImGui::Button("Find Path", ImVec2(-1, 0))) {
        mapViewer.buildPathFromAliases(fromStr, toStr);
    }

    // --- Отрисовка подсказок (простая версия) ---
    const auto& suggestions = mapViewer.getCurrentSuggestions();

    // Временная переменная для отложенного обновления
    std::string suggestionToApply = "";

    if (!suggestions.empty() && ImGui::IsAnyItemActive()) {
        ImGui::Separator();
        ImGui::Text("Suggestions:");
        ImGui::Spacing();

        for (const auto& suggestion : suggestions) {
            // При клике мы НЕ МЕНЯЕМ ДАННЫЕ, а только запоминаем, что нужно сделать.
            if (ImGui::Selectable(suggestion.c_str())) {
                suggestionToApply = suggestion;
            }
        }
    }

    // --- Применение изменений ПОСЛЕ отрисовки ---
    if (!suggestionToApply.empty()) {
        if (mapViewer.isEditingFrom()) {
            fromStr = suggestionToApply;
        }
        else {
            toStr = suggestionToApply;
        }
        // Очищаем подсказки для следующего кадра.
        mapViewer.clearSuggestions();
    }

    ImGui::End();
}

// Stubs remain the same
void UIManager::drawDevInfoWindow(MapViewer& mapViewer) {
    if (!ImGui::Begin("Info & Options")) {
        ImGui::End();
        return;
    }

    // --- Debug Information ---
    ImGui::Text("Debug Info"); ImGui::Separator();
    const Camera& camera = mapViewer.getCamera();
    const SDL_Point& mouseWorld = mapViewer.getDebugMouseWorld();
    ImGui::Text("Zoom: %.0f%%", camera.getScale() * 100.0f);
    ImGui::Text("Mouse World: X=%d, Y=%d", mouseWorld.x, mouseWorld.y);
    ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

    // --- Status Messages ---
    // Показываем сообщение о режиме добавления соседей, если он активен
    if (mapViewer.isNeighborModeActive()) {
        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Neighbor Mode Active for: %s", mapViewer.getNeighborModeActiveId().c_str());
    }

    // Показываем сообщение о сохранении
    Uint32 lastSaveTick = mapViewer.getLastSaveTick();
    if (lastSaveTick != 0 && SDL_GetTicks() - lastSaveTick < 2000) {
        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Graph Saved!");
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    // --- Pathfinding Options ---
    ImGui::Text("Pathfinding Options"); ImGui::Separator();
    ImGui::Checkbox("Allow Stairs [8]", &mapViewer.getUserAllowStairs());
    ImGui::Checkbox("Allow Lifts [9]", &mapViewer.getUserAllowLift());
    ImGui::Checkbox("Allow Bridges [0]", &mapViewer.getUserAllowBridge());

    ImGui::End();
}

void UIManager::drawNodeInspector(MapViewer& mapViewer) {
    const std::string& nodeId = mapViewer.getInspectorNodeId();
    if (nodeId.empty()) {
        return; // Если ни один узел не выбран для инспекции, ничего не делаем
    }

    // Получаем узел из GraphManager'а через MapViewer
    const Node* node = mapViewer.getGraphManager().getNode(nodeId);
    if (!node) {
        // Узел мог быть удален, но ID остался. В этом случае просто перестаем его показывать.
        // Можно добавить метод в MapViewer для сброса ID.
        // mapViewer.clearInspectorNodeId();
        return;
    }

    // Устанавливаем позицию и размер окна инспектора
    ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Node Inspector", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("ID:"); ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", node->id.c_str());

        ImGui::Separator();

        ImGui::Text("Building: %s", node->building.c_str());
        ImGui::Text("Floor: %d", node->floor);
        ImGui::Text("Coords: (%d, %d)", node->x, node->y);
        ImGui::Text("Is Portal: %s", node->isPortal ? "true" : "false");

        ImGui::Separator();

        ImGui::Text("Neighbors (%zu):", node->neighbors.size());
        // Используем дочернее окно со скроллбаром, если соседей много
        if (ImGui::BeginChild("NeighborsList", ImVec2(0, 100), true)) {
            for (const auto& neighborId : node->neighbors) {
                ImGui::Selectable(neighborId.c_str());
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}