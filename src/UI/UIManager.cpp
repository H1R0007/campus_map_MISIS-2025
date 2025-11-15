#include "UIManager.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../config.hpp"
#include <iostream>
#include <string>
#include <algorithm>

// Helpers: std::string-friendly wrappers for ImGui::InputText / InputTextWithHint
// Перегрузки позволяют передавать std::string* вместо char* буфера.
namespace ImGui {
    static int InputTextCallback_Resize(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string* str = static_cast<std::string*>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
        }
        return 0;
    }

    // Обёртка для InputText
    static bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0) {
        flags |= ImGuiInputTextFlags_CallbackResize;
        if (str->capacity() == 0) str->reserve(16);
        return ::ImGui::InputText(label, str->data(), str->capacity() + 1, flags, InputTextCallback_Resize, (void*)str);
    }

    // Обёртка для InputTextWithHint
    static bool InputTextWithHint(const char* label, const char* hint, std::string* str, ImGuiInputTextFlags flags = 0) {
        flags |= ImGuiInputTextFlags_CallbackResize;
        if (str->capacity() == 0) str->reserve(16);
        return ::ImGui::InputTextWithHint(label, hint, str->data(), str->capacity() + 1, flags, InputTextCallback_Resize, (void*)str);
    }
} // namespace ImGui

namespace Layout {
    constexpr float HEADER_HEIGHT = 60.0f;
    constexpr float SIDEBAR_WIDTH = 260.0f;
    constexpr float RIGHTBAR_WIDTH = 340.0f;
    constexpr float WINDOW_MARGIN = 20.0f;
    constexpr float TOP_MARGIN = 20.0f; // единый отступ сверху — на одном уровне с логотипом
}

// --- Конструктор ---
UIManager::UIManager() {}

// --- render() ---
void UIManager::render(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

#ifndef EMSCRIPTEN
    // F12 — показать/скрыть Dev Dock (только в DEV)
    if (Config::DEV_MODE && ImGui::IsKeyPressed(ImGuiKey_F12)) {
        devToolsVisible = !devToolsVisible;
    }

    // Глобальные хоткеи (работают, когда Dev Dock видим)
    if (Config::DEV_MODE && devToolsVisible) {
        bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) viewer.getGraphManager().undoGlobal();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) viewer.getGraphManager().redoGlobal();
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) viewer.getGraphManager().saveActive();
    }
#endif

    // --- Шапка (по центру) ---
    drawTopNavBar(viewer);

    // --- Объединённое окно маршрута: слева, вровень по Oy с логотипом ---
    drawSearchWindow(viewer);

    // --- Панель "Корпус/Этажи" справа, также вровень по Oy с логотипом ---
    drawFloorBuildingPanel(viewer);

    // --- Нижняя меню-панель MISIS ---
    drawBottomMenuBar(viewer);

#ifndef EMSCRIPTEN
    // Компактный Dev Dock
    if (Config::DEV_MODE && devToolsVisible) {
        drawDevDock(viewer);
        // Всегда доступная плавающая кнопка-развёртка (на случай, если мини-окно слишком компактное)
        if (devToolsMini) drawDevDockToggleButton();
    }
#endif
}

// === Объединённое окно маршрута (компактное) ===
void UIManager::drawSearchWindow(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();

    // Адаптивная ширина: компактная, но читаемая
    float screenW = io.DisplaySize.x;
    float panelW = std::clamp(screenW * 0.26f, 260.0f, 340.0f); // ~30% уже прежних 400
    float panelH_collapsed = 90.0f;  // только "Откуда"
    float panelH_expanded = 190.0f; // "Откуда", "Куда", кнопка

    float X = 20.0f;
    float Y = Layout::TOP_MARGIN;    // вровень с логотипом
    float W = panelW;
    float H = routePanelExpanded ? panelH_expanded : panelH_collapsed;

    ImGui::SetNextWindowPos(ImVec2(X, Y));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::SetNextWindowBgAlpha(0.97f);

    if (ImGui::Begin("RoutePanelUnified", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // Фон + рамка
        const float RADIUS = 12.0f;
        ImVec4 bg = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 border = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::GetColorU32(bg), RADIUS);
        dl->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::GetColorU32(border), RADIUS, 0, 1.2f);

        // Заголовок
        ImGui::SetCursorPos(ImVec2(16, 10));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf4d7  Маршрут"); // FA "route"
        ImGui::PopStyleColor();

        // Кнопка сворачивания — абсолютная позиция внутри контента (не обрежется)
        ImVec2 cMin = ImGui::GetWindowContentRegionMin();
        ImVec2 cMax = ImGui::GetWindowContentRegionMax();
        const float pad = 6.0f;
        ImVec2 chevronPos(winPos.x + cMax.x - 24.0f - pad, winPos.y + cMin.y + pad);
        ImGui::SetCursorScreenPos(chevronPos);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        const char* chevron = routePanelExpanded ? u8"\uf077" : u8"\uf078"; // up/down
        if (ImGui::Button(chevron, ImVec2(24, 24))) routePanelExpanded = !routePanelExpanded;
        ImGui::PopStyleVar();

        // Поля ввода
        std::string& fromStr = mapViewer.getInputFrom();
        std::string& toStr = mapViewer.getInputTo();

        auto pushFieldStyle = [] {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.96f, 0.98f, 1.0f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.22f, 1.0f));
            };
        auto popFieldStyle = [] {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            };

        ImGui::PushItemWidth(-1);

        // ОТКУДА
        ImVec2 fromMin, fromMax;
        ImGui::SetCursorPos(ImVec2(16, 44));
        pushFieldStyle();
        bool fromChanged = ImGui::InputTextWithHint("##fromRouteUnified", "Откуда...", &fromStr);
        popFieldStyle();
        fromMin = ImGui::GetItemRectMin();
        fromMax = ImGui::GetItemRectMax();
        if (ImGui::IsItemActive()) {
            dl->AddRect(fromMin, fromMax, ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)), 6.0f, 0, 2.0f);
            mapViewer.setEditingFrom(true);
        }

        // СВЕРНУТО: только "Откуда" + разворот по клику + подсказки
        if (!routePanelExpanded) {
            bool clickedFromField =
                ImGui::IsItemClicked(ImGuiMouseButton_Left);
            bool clickedInsideWindow =
                ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            if (clickedFromField || clickedInsideWindow) {
                routePanelExpanded = true;
            }

            auto drawSuggestionsPopup = [&](const std::vector<std::string>& s,
                const ImVec2& fieldMin,
                const ImVec2& fieldMax) {
                    if (s.empty()) return;
                    for (const std::string& v : s) if (v == fromStr) return;

                    const float popupW = 210.0f;
                    ImGui::SetNextWindowPos(ImVec2(fieldMax.x + 12.0f, fieldMin.y));
                    ImGui::SetNextWindowSize(ImVec2(popupW, 120.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1, 1, 1, 0.97f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1));

                    bool open = true;
                    if (ImGui::Begin("##RouteSuggestionsCollapsed", &open,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))
                    {
                        ImGui::Dummy(ImVec2(10, 6));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
                        ImGui::SetCursorPosX(12);
                        ImGui::TextUnformatted(u8"\uf002  Подсказки");
                        ImGui::PopStyleColor();
                        ImGui::Dummy(ImVec2(0, 4));

                        const float totalW = popupW - 20.0f;
                        ImGui::PushItemWidth(totalW);
                        for (size_t i = 0; i < s.size(); ++i) {
                            std::string id = s[i] + "##collapsed" + std::to_string(i);
                            if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_None, ImVec2(totalW, 24.0f))) {
                                mapViewer.getInputFrom() = s[i];
                                mapViewer.setEditingFrom(true);
                                open = false;
                                routePanelExpanded = true;
                            }
                        }
                        ImGui::PopItemWidth();

                        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
                            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            open = false;
                    }
                    ImGui::End();
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();

                    if (!open) ImGui::SetKeyboardFocusHere(-1);
                };

            if (fromChanged) mapViewer.updateSuggestions();
            mapViewer.setEditingFrom(true);
            drawSuggestionsPopup(mapViewer.getCurrentSuggestions(), fromMin, fromMax);

            ImGui::PopItemWidth();
            ImGui::End();
            return;
        }

        // РАЗВЕРНУТО
        ImGui::Dummy(ImVec2(0, 6));

        ImVec2 toMin, toMax;
        ImGui::SetCursorPos(ImVec2(16, 80));
        pushFieldStyle();
        bool toChanged = ImGui::InputTextWithHint("##toRouteUnified", "Куда...", &toStr);
        popFieldStyle();
        toMin = ImGui::GetItemRectMin();
        toMax = ImGui::GetItemRectMax();
        if (ImGui::IsItemActive()) {
            dl->AddRect(toMin, toMax, ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)), 6.0f, 0, 2.0f);
            mapViewer.setEditingFrom(false);
        }

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.03f, 0.47f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.45f, 0.95f, 1.0f));
        ImGui::SetCursorPos(ImVec2(16, 118));
        if (ImGui::Button(u8"\uf135  Построить маршрут", ImVec2(W - 32.0f, 34.0f))) {
            mapViewer.buildPathFromAliases(fromStr, toStr);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        auto drawSuggestionsPopup = [&](const std::vector<std::string>& sugg,
            const ImVec2& fieldMin,
            const ImVec2& fieldMax,
            bool forFrom,
            std::string& fieldText)
            {
                if (sugg.empty()) return;
                for (const std::string& s : sugg) if (s == fieldText) return;

                const float popupW = 220.0f;
                ImGui::SetNextWindowPos(ImVec2(fieldMax.x + 12.0f, fieldMin.y));
                ImGui::SetNextWindowSize(ImVec2(popupW, 140.0f));

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1, 1, 1, 0.97f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1.0f));

                bool open = true;
                if (ImGui::Begin(forFrom ? "##RouteSuggestionsFromUnified" : "##RouteSuggestionsToUnified", &open,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoBringToFrontOnFocus))
                {
                    ImGui::Dummy(ImVec2(10, 6));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
                    ImGui::SetCursorPosX(12);
                    ImGui::TextUnformatted(u8"\uf002  Подсказки");
                    ImGui::PopStyleColor();
                    ImGui::Dummy(ImVec2(0, 4));

                    const float totalW = popupW - 20.0f;
                    ImGui::PushItemWidth(totalW);
                    for (size_t i = 0; i < sugg.size(); ++i) {
                        std::string id = sugg[i] + "##" + std::to_string(i) +
                            (forFrom ? "##from" : "##to");
                        if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_None, ImVec2(totalW, 24.0f))) {
                            if (forFrom) {
                                mapViewer.getInputFrom() = sugg[i];
                                mapViewer.setEditingFrom(true);
                            }
                            else {
                                mapViewer.getInputTo() = sugg[i];
                                mapViewer.setEditingFrom(false);
                            }
                            open = false;
                        }
                    }
                    ImGui::PopItemWidth();

                    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        open = false;
                }
                ImGui::End();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();

                if (!open) ImGui::SetKeyboardFocusHere(-1);
            };

        if (fromChanged || toChanged) mapViewer.updateSuggestions();
        if (mapViewer.isEditingFrom())
            drawSuggestionsPopup(mapViewer.getCurrentSuggestions(), fromMin, fromMax, true, fromStr);
        else
            drawSuggestionsPopup(mapViewer.getCurrentSuggestions(), toMin, toMax, false, toStr);

        ImGui::PopItemWidth();
    }
    ImGui::End();
}

// === Панель выбора корпуса/этажей (компактная, справа) ===
void UIManager::drawFloorBuildingPanel(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    // Геометрия окна — справа, вровень по Oy с логотипом
    float PANEL_W = std::clamp(screenW * 0.20f, 240.0f, 300.0f);
    float BUTTON_H = 28.0f;
    float RADIUS = 12.0f;

    // вычисляем количество этажей для динамической высоты
    int floorCount = 0;
    {
        const BuildingMeta* meta = mapViewer.getGraphManager()
            .getBuildingMeta(mapViewer.getCurrentBuilding());
        if (meta) floorCount = static_cast<int>(meta->floors.size());
    }
    float PANEL_H = 120.0f + floorCount * (BUTTON_H + 6.0f);

    float panelX = screenW - PANEL_W - 20.0f;
    float panelY = Layout::TOP_MARGIN;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(PANEL_W, PANEL_H));
    ImGui::SetNextWindowBgAlpha(0.97f);

    if (ImGui::Begin("BuildingFloorsCompact", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        // фон и рамка
        ImVec4 bg = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 border = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bg), RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(border), RADIUS, 0, 1.3f);

        ImGui::SetCursorPos(ImVec2(16, 12));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf1ad  Корпус"); // building icon
        ImGui::PopStyleColor();

        // Фальш-комбобокс корпусов (тоггл)
        const auto& metas = mapViewer.getGraphManager().buildingMetas;
        std::string currentBuilding = mapViewer.getCurrentBuilding();

        const char* currentLabel = currentBuilding.empty()
            ? "— выберите корпус —"
            : currentBuilding.c_str();

        static bool popupOpen = false;

        ImGui::SetCursorPos(ImVec2(16, 42));
        ImGui::PushItemWidth(PANEL_W - 32);

        bool clickedButton = ImGui::Button(currentLabel, ImVec2(PANEL_W - 32, 28));
        ImVec2 btnMin = ImGui::GetItemRectMin();
        ImVec2 btnMax = ImGui::GetItemRectMax();
        if (clickedButton) popupOpen = !popupOpen;

        // Стрелка справа — через draw list
        ImU32 arrowCol = ImGui::GetColorU32(ImVec4(0.35f, 0.42f, 0.55f, 1.0f));
        const char* chevron = popupOpen ? u8"\uf077" : u8"\uf078"; // up/down
        ImVec2 arrowPos(btnMax.x - 22.0f, btnMin.y + 5.0f);
        dl->AddText(arrowPos, arrowCol, chevron);

        ImGui::PopItemWidth();

        // POPUP корпусов
        if (popupOpen) {
            const float popupW = PANEL_W - 40.0f;
            const float popupH = 240.0f;

            ImGui::SetNextWindowPos(ImVec2(btnMin.x, btnMax.y + 6.0f));
            ImGui::SetNextWindowSize(ImVec2(popupW, popupH));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 0.97f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1.0f));

            if (ImGui::Begin("##BuildingListPopup", &popupOpen,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::Dummy(ImVec2(10, 6));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
                ImGui::SetCursorPosX(12);
                ImGui::TextUnformatted(u8"\uf1ad  Выберите корпус");
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0, 6));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 6));

                const float totalW = popupW - 20.0f;
                ImGui::PushItemWidth(totalW);

                for (const auto& [bid, meta] : metas) {
                    bool selected = (bid == currentBuilding);
                    ImGui::SetCursorPosX(10);
                    if (ImGui::Selectable(meta.name.c_str(), selected,
                        ImGuiSelectableFlags_None, ImVec2(totalW, 26.0f)))
                    {
                        if (!meta.floors.empty())
                            mapViewer.switchToFloor(bid, meta.floors.front().floor);
                        popupOpen = false;
                    }
                }

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 4));

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.93f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.90f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.87f, 1.0f, 1.0f));
                ImGui::SetCursorPosX(10);
                if (ImGui::Button(u8"\uf279  Вид сверху (кампус)", ImVec2(totalW, 30.0f))) { // FA "map"
                    mapViewer.switchViewToCampus();
                    popupOpen = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();

                ImGui::PopItemWidth();

                if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    popupOpen = false;
            }
            ImGui::End();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        // Этажи
        const BuildingMeta* bm = mapViewer.getGraphManager().getBuildingMeta(currentBuilding);
        if (bm && !bm->floors.empty()) {
            ImGui::SetCursorPos(ImVec2(16, 84));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.18f, 0.26f, 1.0f));
            ImGui::TextUnformatted("Этажи:");
            ImGui::PopStyleColor();

            std::vector<int> floors;
            for (const auto& f : bm->floors) floors.push_back(f.floor);
            std::sort(floors.begin(), floors.end(), std::greater<int>());

            float y = 104.0f;
            for (int f : floors) {
                ImGui::SetCursorPos(ImVec2(24, y));
                bool active = (f == mapViewer.getCurrentFloor());
                ImVec4 col = active
                    ? ImVec4(0.08f, 0.50f, 1.0f, 1.0f)
                    : ImVec4(1.0f, 1.0f, 1.0f, 0.95f);
                ImGui::PushStyleColor(ImGuiCol_Button, col);
                std::string label = "Этаж " + std::to_string(f);
                if (ImGui::Button(label.c_str(), ImVec2(PANEL_W - 48, BUTTON_H))) {
                    mapViewer.switchToFloor(currentBuilding, f);
                }
                ImGui::PopStyleColor();
                y += BUTTON_H + 6.0f;
            }
        }
        else {
            ImGui::SetCursorPos(ImVec2(16, 90));
            ImGui::TextDisabled("Этажи отсутствуют");
        }
    }
    ImGui::End();
}

// === Компактная верхняя панель MISIS — финальный вариант ===
void UIManager::drawTopNavBar(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    // Базовые параметры адаптивного окна
    constexpr float PANEL_H_BASE = 90.0f;
    constexpr float SHRINK_Y = 0.8f;
    constexpr float EXPAND_X = 1.2f;
    constexpr float LOGO_MAX_H = 68.0f;
    constexpr float BTN_SIZE = 36.0f;

    // ширина панели от реального логотипа
    float baseW = 320.0f;
    if (logoTexture) {
        int w, h;
        SDL_QueryTexture(logoTexture, nullptr, nullptr, &w, &h);
        if (w > 0 && h > 0)
            baseW = static_cast<float>(w) * (LOGO_MAX_H / h) + 120.0f;
    }

    float panelW = baseW * EXPAND_X;
    float panelH = PANEL_H_BASE * SHRINK_Y;
    float panelX = (screenW - panelW) * 0.5f;
    float panelY = Layout::TOP_MARGIN; // выравниваем по Oy

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    if (ImGui::Begin("TopBarMISIS_Compact", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        const float RADIUS = 12.0f;
        ImVec4 bgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 borderColor = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);

        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bgColor), RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(borderColor), RADIUS, 0, 1.6f);

        float centerY = pos.y + size.y * 0.5f;

        // Логотип
        if (logoTexture) {
            int tw, th;
            SDL_QueryTexture(logoTexture, nullptr, nullptr, &tw, &th);
            float scale = LOGO_MAX_H / static_cast<float>(th);
            ImVec2 logoSize(tw * scale, th * scale);
            float logoY = centerY - logoSize.y * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f, logoY));
            ImGui::Image((ImTextureID)(intptr_t)logoTexture, logoSize);
        }
        else {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 20.0f, centerY - 10.0f));
            ImGui::TextColored(ImVec4(0.0f, 0.35f, 0.75f, 1.0f), "MISIS");
        }

        // Решаем, показывать ли fullscreen на этом устройстве
#ifdef EMSCRIPTEN
        const bool showFullscreen = false;
#else
        const bool showFullscreen = (io.DisplaySize.x >= 1000.0f && io.DisplaySize.y >= 650.0f);
#endif

        int btnCount = showFullscreen ? 3 : 2; // добавили шестерёнку
        float totalBtnW = BTN_SIZE * btnCount + (btnCount - 1) * 14.0f;

        float btnStartX = pos.x + size.x - totalBtnW - 16.0f;
        float btnY = centerY - BTN_SIZE * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(btnStartX, btnY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.96f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.91f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.86f, 1.0f, 1.0f));

        // 🌐 (FA: \uf1ab)
        if (ImGui::Button(u8"\uf1ab", ImVec2(BTN_SIZE, BTN_SIZE))) {
            SDL_Log("[UI] Language switch clicked");
        }

        // ⚙️ Dev Dock (только в DEV)
#ifndef EMSCRIPTEN
        if (Config::DEV_MODE) {
            ImGui::SameLine(0, 14.0f);
            if (ImGui::Button(u8"\uf013", ImVec2(BTN_SIZE, BTN_SIZE))) { // FA cog
                devToolsVisible = !devToolsVisible;
            }
        }
#endif

        // ⛶ Fullscreen (только на десктопе и больших экранах)
        if (showFullscreen) {
            ImGui::SameLine(0, 14.0f);
            if (ImGui::Button(u8"\uf065", ImVec2(BTN_SIZE, BTN_SIZE))) {
                SDL_Window* win = SDL_GL_GetCurrentWindow();
                if (win) {
                    Uint32 flags = SDL_GetWindowFlags(win);
                    SDL_SetWindowFullscreen(win,
                        (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

// === Нижняя центральная панель навигации MISIS ===
void UIManager::drawBottomMenuBar(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    // Геометрия панели
    const float PANEL_H = 80.0f;
    const float PANEL_MARGIN_BOTTOM = 20.0f;
    const float PANEL_RADIUS = 16.0f;

    // Список элементов: {иконка, подпись}
    static const std::pair<const char*, const char*> items[] = {
        { u8"\uf2e7", "Столовые"    },
        { u8"\uf02d", "Библиотеки"  },
        { u8"\uf0f4", "Кофейни"     },
        { u8"\uf19c", "Аудитории"   },
        { u8"\uf1eb", "Wi‑Fi зоны"  },
        { u8"\uf059", "Справка"     }
    };

    const int COUNT = IM_ARRAYSIZE(items);
    const float ICON_SIZE = 32.0f;
    const float BLOCK_W = 100.0f;
    const float BLOCK_SPACING = 60.0f; // расстояние между блоками
    const float PANEL_W = (COUNT * BLOCK_W + (COUNT - 1) * BLOCK_SPACING);

    float panelX = (screenW - PANEL_W) * 0.5f;
    float panelY = screenH - PANEL_H - PANEL_MARGIN_BOTTOM;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(PANEL_W, PANEL_H), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);

    if (ImGui::Begin("BottomNavMISIS", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        ImVec4 bgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 borderColor = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bgColor), PANEL_RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(borderColor), PANEL_RADIUS, 0, 1.5f);

        float midY = pos.y + size.y * 0.5f;

        ImVec4 defaultIconCol = ImVec4(0.06f, 0.40f, 0.95f, 1.0f);  // фирменный синий
        ImVec4 activeIconCol = ImVec4(0.00f, 0.60f, 1.0f, 1.0f);    // голубой ярче

        float startX = pos.x + BLOCK_W * 0.5f;

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.93f, 1.0f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.90f, 1.0f, 0.5f));

        for (int i = 0; i < COUNT; ++i) {
            const char* icon = items[i].first;
            const char* label = items[i].second;

            float blockX = startX + i * (BLOCK_W + BLOCK_SPACING);
            float iconY = midY - ICON_SIZE * 0.9f;
            float textY = iconY + ICON_SIZE + 6.0f;

            // Иконка
            ImGui::SetCursorScreenPos(ImVec2(blockX - ICON_SIZE * 0.5f, iconY));
            ImVec4 col = ImGui::IsMouseHoveringRect(ImVec2(blockX - 20, iconY - 4),
                ImVec2(blockX + 20, textY + 10))
                ? activeIconCol : defaultIconCol;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Button(icon, ImVec2(ICON_SIZE, ICON_SIZE));
            ImGui::PopStyleColor();

            // Текст
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.15f, 0.22f, 1.0f));
            ImVec2 textSize = ImGui::CalcTextSize(label);
            ImGui::SetCursorScreenPos(ImVec2(blockX - textSize.x * 0.5f, textY));
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

// === USER Right Panel (если нужна). В DEV используем Dev Dock. ===
void UIManager::drawRightPanel(MapViewer& viewer) {
    // Оставим как есть или используем только для USER режима. В DEV режим основной — Dev Dock.
    // Чтобы не дублировать, панель можно не вызывать из render(), как сейчас.
}

// === Dev Dock ===
void UIManager::drawDevDock(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
    if (devToolsMini) {
        // В мини-режиме запрещаем ресайз, фиксируем высоту
        ImGui::SetNextWindowSize(ImVec2(300, 72), ImGuiCond_Always);
    }
    else {
        ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
    }

    // Спавним окно в правом нижнем углу при первом появлении
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, io.DisplaySize.y - 16),
        ImGuiCond_FirstUseEver, ImVec2(1, 1));

    if (ImGui::Begin("Dev Dock", nullptr, flags)) {
        // Заголовок + мини-контролы (всегда видимы)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.35f, 0.8f, 1.0f));
        ImGui::TextUnformatted(u8"\uf188  Dev Tools"); // FA bug
        ImGui::PopStyleColor();

        ImGui::SameLine();
        float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SetCursorPosX(right - (devToolsMini ? 120.0f : 140.0f));
        if (ImGui::SmallButton(devToolsMini ? u8"\uf065 expand" : u8"\uf066 compact")) { // expand/compress
            devToolsMini = !devToolsMini;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"\uf00d close")) { // close
            devToolsVisible = false;
            ImGui::End();
            return;
        }

        // MINI режим — только быстрые кнопки
        if (devToolsMini) {
            ImGui::Separator();
            if (ImGui::Button(u8"\uf0e2 Undo", ImVec2(88, 26))) viewer.getGraphManager().undoGlobal();
            ImGui::SameLine();
            if (ImGui::Button(u8"\uf01e Redo", ImVec2(88, 26))) viewer.getGraphManager().redoGlobal();
            ImGui::SameLine();
            if (ImGui::Button(u8"\uf0c7 Save", ImVec2(88, 26))) viewer.getGraphManager().saveActive();

            ImGui::Separator();
            ImGui::Checkbox("Draw Nodes", &viewer.getDebugDrawNodes());
            ImGui::SameLine();
            ImGui::Text("FPS %.1f", io.Framerate);

            ImGui::End();
            return;
        }

        // FULL режим
        ImGui::Separator();
        if (ImGui::BeginTabBar("DevDockTabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            // ==== Actions ====
            if (ImGui::BeginTabItem(u8"\uf135 Actions")) { // rocket
                if (ImGui::Button(u8"\uf0e2  Undo [Ctrl+Z]", ImVec2(-1, 28))) viewer.getGraphManager().undoGlobal();
                if (ImGui::Button(u8"\uf01e  Redo [Ctrl+Y]", ImVec2(-1, 28))) viewer.getGraphManager().redoGlobal();
                ImGui::Separator();
                if (ImGui::Button(u8"\uf0c7  Save Active Graph [Ctrl+S]", ImVec2(-1, 28))) viewer.getGraphManager().saveActive();
                if (ImGui::Button(u8"\uf0c7  Save Transitions", ImVec2(-1, 28))) viewer.getGraphManager().saveTransitions(Config::TRANSITIONS_PATH);
                ImGui::Separator();

                bool clean = viewer.getGraphManager().history.isAtSavepoint();
                ImGui::Text("History: undo=%zu redo=%zu  |  %s",
                    viewer.getGraphManager().history.undoSize(),
                    viewer.getGraphManager().history.redoSize(),
                    clean ? "Saved" : "Modified");
                if (ImGui::Button("Mark Savepoint", ImVec2(-1, 24)))
                    viewer.getGraphManager().history.markSavepoint();
                if (ImGui::Button("Dump History to file", ImVec2(-1, 24)))
                    viewer.getGraphManager().history.dumpToFile("history_dump.json");
                ImGui::EndTabItem();
            }

            // ==== Info ====
            if (ImGui::BeginTabItem(u8"\uf05a Info")) { // info-circle
                const Camera& cam = viewer.getCamera();
                ImGui::Text("Zoom: %.0f%%", cam.getScale() * 100.0f);
                ImGui::Text("FPS:  %.1f", io.Framerate);
                ImGui::Text("Size: %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
                ImGui::Separator();
                ImGui::Checkbox("Draw Nodes", &viewer.getDebugDrawNodes());
                ImGui::Checkbox("Allow Stairs [8]", &viewer.getUserAllowStairs());
                ImGui::Checkbox("Allow Lifts [9]", &viewer.getUserAllowLift());
                ImGui::Checkbox("Allow Bridges [0]", &viewer.getUserAllowBridge());

                // History capacity
                size_t cap = viewer.getGraphManager().history.getMaxEntries();
                int capInt = static_cast<int>(cap);
                ImGui::SliderInt("History Capacity", &capInt, 50, 1000, "%d");
                if (capInt != (int)cap) viewer.getGraphManager().history.setMaxEntries((size_t)capInt);

                ImGui::EndTabItem();
            }

            // ==== Inspector ====
            if (ImGui::BeginTabItem(u8"\uf002 Inspector")) { // search
                const std::string& id = viewer.getInspectorNodeId();
                if (id.empty()) {
                    ImGui::TextDisabled("No node selected.");
                }
                else {
                    const Node* n = viewer.getGraphManager().getNode(id);
                    if (n) {
                        ImGui::Text("ID: %s", n->id.c_str());
                        ImGui::Text("Pos: (%d, %d)", n->x, n->y);
                        ImGui::Text("Floor: %d", n->floor);
                        ImGui::Text("Building: %s", n->building.c_str());
                        ImGui::Text("Neighbors: %zu", n->neighbors.size());
                    }
                    else {
                        ImGui::TextDisabled("Node not found.");
                    }
                }
                ImGui::EndTabItem();
            }

            // ==== Goto (focus camera) ====
            if (ImGui::BeginTabItem(u8"\uf124 Goto")) { // location-arrow
                static std::string gotoStr;
                ImGui::InputTextWithHint("##gotoNode", "node id / alias", &gotoStr);
                ImGui::SameLine();
                if (ImGui::Button("Go", ImVec2(60, 0))) {
                    std::string id = viewer.getAliasManager().resolve(gotoStr);
                    if (id.empty()) id = gotoStr;
                    viewer.requestFocusToNode(id, 0.28f);
                }
                ImGui::TextDisabled("Hint: accepts alias or id. Focuses camera softly.");
                ImGui::EndTabItem();
            }

            // ==== Console ====
            if (ImGui::BeginTabItem(u8"\uf120 Console")) { // terminal
                static char buf[512] = "";
                static std::vector<std::string> log;
                ImGui::BeginChild("log", ImVec2(-1, 180), true);
                for (auto& l : log) ImGui::TextUnformatted(l.c_str());
                ImGui::EndChild();
                if (ImGui::InputText("##cmd", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    std::string cmd = buf;
                    if (cmd == "clear") log.clear();
                    else if (cmd == "undo") viewer.getGraphManager().undoGlobal();
                    else if (cmd == "redo") viewer.getGraphManager().redoGlobal();
                    else if (cmd == "save") viewer.getGraphManager().saveActive();
                    else if (cmd == "save_tr") viewer.getGraphManager().saveTransitions(Config::TRANSITIONS_PATH);
                    else if (cmd == "mark_savepoint") viewer.getGraphManager().history.markSavepoint();
                    else log.push_back("> " + cmd);
                    buf[0] = 0;
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// === Плавающая кнопка-развёртка для Mini режима (чтобы всегда была доступна) ===
void UIManager::drawDevDockToggleButton() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 18.0f, io.DisplaySize.y - 96.0f),
        ImGuiCond_Always, ImVec2(1, 1));
    ImGui::SetNextWindowBgAlpha(0.0f); // полностью прозрачный фон
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("DevDockToggle", nullptr, flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.50f, 1.00f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.60f, 1.00f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.45f, 0.90f, 0.95f));
        if (ImGui::Button(u8"\uf065", ImVec2(36, 36))) { // expand icon
            devToolsMini = false;
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

// (Не используется напрямую сейчас)
void UIManager::drawDevInfoWindow(MapViewer& mapViewer) {
    // Оставлено пустым — DevDock покрывает потребности.
}