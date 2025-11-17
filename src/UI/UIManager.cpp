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

    drawLogoOverlay();

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
    }
#endif
    drawToasts();
}

// === Объединённое окно маршрута (компактное) ===
void UIManager::drawSearchWindow(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& st = ImGui::GetStyle();

    // — геометрия и адаптив —
    float screenW = io.DisplaySize.x;
    float panelW = std::clamp(screenW * 0.26f, 260.0f, 340.0f); // компактная ширина
    const float PAD = 16.0f;

    // параметры “шапки” внутри окна
    const float headerY = 10.0f;
    const float headerH = 28.0f;
    const float frameH = ImGui::GetFrameHeight(); // высота поля ввода
    const float spacingY = st.ItemSpacing.y * 1.10f + 4.0f; // +10% вертикального отступа

    // посчитаем высоты окна для режимов
    float collapsedH = headerY + headerH + 6.0f + frameH + PAD; // заголовок + одно поле + паддинг
    float buttonH = 34.0f;
    float expandedH = headerY + headerH + 6.0f + frameH + spacingY + frameH + spacingY + buttonH + PAD;

    float X = 20.0f;
    float Y = Layout::TOP_MARGIN;
    float W = panelW;
    float H = (routePanelExpanded ? expandedH : collapsedH);

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

        // фон + рамка карточки
        const float RADIUS = 12.0f;
        ImVec4 bg = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 border = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::GetColorU32(bg), RADIUS);
        dl->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::GetColorU32(border), RADIUS, 0, 1.2f);

        // Заголовок слева
        ImGui::SetCursorPos(ImVec2(PAD, headerY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf4d7  Маршрут"); // FA "route"
        ImGui::PopStyleColor();

        // Кнопка сворачивания (chevron) — в правом верхнем углу контента, компактная
        {
            const float btnSz = 20.0f;     // компактно как раньше
            const float pad = 6.0f;

            ImVec2 cMax = ImGui::GetWindowContentRegionMax();
            ImVec2 localPos(cMax.x - btnSz - pad, headerY); // локальные координаты
            ImGui::SetCursorPos(localPos);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            const char* label = routePanelExpanded ? u8"\uf077" : u8"\uf078"; // FA up/down
            if (ImGui::Button(label, ImVec2(btnSz, btnSz))) {
                routePanelExpanded = !routePanelExpanded;
                // Мгновенно применяем новый размер — нет «возврата» к сжатому
                float newH = routePanelExpanded ? expandedH : collapsedH;
                ImGui::SetWindowSize(ImVec2(W, newH));
            }
            ImGui::PopStyleVar(2);
        }

        // Общий стиль полей
        auto pushFieldStyle = [] {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.96f, 0.98f, 1.0f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.22f, 1.0f));
            };
        auto popFieldStyle = [] {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            };

        // Поля ввода
        std::string& fromStr = mapViewer.getInputFrom();
        std::string& toStr = mapViewer.getInputTo();
        ImGui::PushItemWidth(-1);

        // ОТКУДА — координаты и рисование
        ImVec2 fromMin, fromMax;
        {
            float y = headerY + headerH + 6.0f;
            ImGui::SetCursorPos(ImVec2(PAD, y));
            pushFieldStyle();
            bool fromChanged = ImGui::InputTextWithHint("##fromRouteUnified", "Откуда...", &fromStr);
            popFieldStyle();
            fromMin = ImGui::GetItemRectMin();
            fromMax = ImGui::GetItemRectMax();

            // лёгкая рамка фона для неактивного поля (лучше читается на белом)
            bool fromActive = ImGui::IsItemActive();
            ImU32 subtle = ImGui::GetColorU32(ImVec4(0.15f, 0.20f, 0.35f, fromActive ? 0.00f : 0.10f));
            dl->AddRect(fromMin, fromMax, subtle, 6.0f, 0, 1.0f);

            // активная обводка
            if (fromActive) {
                dl->AddRect(fromMin, fromMax,
                    ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)), 6.0f, 0, 2.0f);
                mapViewer.setEditingFrom(true);
            }

            if (fromChanged) mapViewer.updateSuggestions();

            // СВЕРНУТО: только "Откуда" + разворот по клику + подсказки
            if (!routePanelExpanded) {
                bool clickedFromField = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                bool clickedInsideWindow =
                    ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                if (clickedFromField || clickedInsideWindow) {
                    routePanelExpanded = true;
                    float newH = expandedH;
                    ImGui::SetWindowSize(ImVec2(W, newH));
                }

                // Popup подсказок — ВСЕГДА справа от поля (шапка убрана)
                auto drawCollapsedSuggestions = [&](const std::vector<std::string>& s,
                    const ImVec2& fieldMin,
                    const ImVec2& fieldMax)
                    {
                        if (s.empty()) return;
                        for (const std::string& v : s) if (v == fromStr) return;

                        float popupW = 210.0f;
                        ImVec2 pos(fieldMax.x + 12.0f, fieldMin.y);

                        ImGui::SetNextWindowPos(pos);
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
                                    ImGui::SetWindowSize(ImVec2(W, expandedH));
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

                mapViewer.setEditingFrom(true);
                drawCollapsedSuggestions(mapViewer.getCurrentSuggestions(), fromMin, fromMax);

                ImGui::PopItemWidth();
                ImGui::End();
                return;
            }
        }

        // РАЗВЕРНУТО: КУДА
        ImVec2 toMin, toMax;
        {
            float y2 = headerY + headerH + 6.0f + frameH + spacingY;
            ImGui::SetCursorPos(ImVec2(PAD, y2));
            pushFieldStyle();
            bool toChanged = ImGui::InputTextWithHint("##toRouteUnified", "Куда...", &toStr);
            popFieldStyle();
            toMin = ImGui::GetItemRectMin();
            toMax = ImGui::GetItemRectMax();

            bool toActive = ImGui::IsItemActive();
            ImU32 subtle = ImGui::GetColorU32(ImVec4(0.15f, 0.20f, 0.35f, toActive ? 0.00f : 0.10f));
            dl->AddRect(toMin, toMax, subtle, 6.0f, 0, 1.0f);

            if (toActive) {
                dl->AddRect(toMin, toMax,
                    ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)), 6.0f, 0, 2.0f);
                mapViewer.setEditingFrom(false);
            }

            if (toChanged) mapViewer.updateSuggestions();
        }

        // Кнопка "Построить маршрут"
        {
            float yBtn = toMax.y - ImGui::GetWindowPos().y + spacingY;
            ImGui::SetCursorPos(ImVec2(PAD, yBtn));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.03f, 0.47f, 1.00f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.45f, 0.95f, 1.0f));
            if (ImGui::Button(u8"\uf135  Построить маршрут", ImVec2(W - PAD * 2.0f, 34.0f))) {
                mapViewer.buildPathFromAliases(fromStr, toStr);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }

        // Подсказки — для активного поля, ВСЕГДА справа (шапка снята)
        auto drawSuggestionsPopup = [&](const std::vector<std::string>& sugg,
            const ImVec2& fieldMin,
            const ImVec2& fieldMax,
            bool forFrom,
            std::string& fieldText)
            {
                if (sugg.empty()) return;
                for (const std::string& s : sugg) if (s == fieldText) return;

                float popupW = std::clamp(W - PAD * 2.0f, 220.0f, 320.0f);
                ImVec2 pos(fieldMax.x + 12.0f, fieldMin.y);

                ImGui::SetNextWindowPos(pos);
                ImGui::SetNextWindowSize(ImVec2(popupW, 140.0f));

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1, 1, 1, 0.97f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1.0f));

                bool open = true;
                std::string winId = forFrom ? "##RouteSuggestionsFromUnified" : "##RouteSuggestionsToUnified";
                if (ImGui::Begin(winId.c_str(), &open,
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

                    ImGui::PushItemWidth(popupW - 20.0f);
                    for (size_t i = 0; i < sugg.size(); ++i) {
                        std::string id = sugg[i] + "##" + std::to_string(i) + (forFrom ? "##from" : "##to");
                        if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_None, ImVec2(popupW - 20.0f, 24.0f))) {
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

        // актуализируем подсказки (MapViewer сам оптимизирует перерасчёт)
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

void UIManager::drawLogoOverlay() {
    if (!logoTexture) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Размер логотипа — прежний по высоте (не уменьшать)
    int tw = 0, th = 0;
    SDL_QueryTexture(logoTexture, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) return;

    const float LOGO_H = 68.0f; // как в шапке
    float scale = LOGO_H / static_cast<float>(th);
    ImVec2 size(static_cast<float>(tw) * scale, LOGO_H);

    // Позиция — топ‑центр, с небольшим отступом сверху
    const float topPad = Layout::TOP_MARGIN; // 20
    ImVec2 pos((io.DisplaySize.x - size.x) * 0.5f, topPad);

    // Лёгкая подложка‑тень для читаемости на светлой карте
    ImU32 bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.85f));
    ImU32 bd = ImGui::GetColorU32(ImVec4(0.82f, 0.84f, 0.85f, 1.0f));
    ImVec2 p1 = pos;
    ImVec2 p2 = ImVec2(pos.x + size.x, pos.y + size.y);

    // Подложку можно отключить, если хочется «чистый» логотип
    // dl->AddRectFilled(p1, p2, bg, 10.0f);
    // dl->AddRect(p1, p2, bd, 10.0f, 0, 1.5f);

    // Сам логотип
    ImTextureID tex = (ImTextureID)(intptr_t)logoTexture;
    dl->AddImage(tex, p1, p2);
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


// === Dev Dock ===
void UIManager::drawDevDock(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();

    // Минимальные размеры, чтобы кнопки в хедере всегда были видны
    if (devToolsMini) {
        ImGui::SetNextWindowSize(ImVec2(320, 76), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 70), ImVec2(FLT_MAX, FLT_MAX));
    }
    else {
        ImGui::SetNextWindowSize(ImVec2(460, 460), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420, 360), ImVec2(FLT_MAX, FLT_MAX));
    }

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, io.DisplaySize.y - 16),
        ImGuiCond_FirstUseEver, ImVec2(1, 1));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Dev Dock", nullptr, flags)) {
        // Заголовок + кнопки
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.35f, 0.8f, 1.0f));
        ImGui::TextUnformatted(u8"\uf188  Dev Tools");
        ImGui::PopStyleColor();

        // Кнопки справа: compact/expand + close (иконки — короткие, чтобы поместились)
        ImGui::SameLine();
        float right = ImGui::GetWindowContentRegionMax().x;
        const float btnW = 72.0f;
        ImGui::SetCursorPosX(std::max(0.0f, right - (btnW * 2 + 8.0f)));
        if (ImGui::SmallButton(devToolsMini ? u8"\uf065 expand" : u8"\uf066 compact")) {
            devToolsMini = !devToolsMini;
        }
        ImGui::SameLine(0, 8);
        if (ImGui::SmallButton(u8"\uf00d close")) {
            devToolsVisible = false;
            ImGui::End();
            return;
        }

        if (devToolsMini) {
            ImGui::Separator();
            if (ImGui::Button(u8"\uf0e2 Undo", ImVec2(88, 26))) { viewer.getGraphManager().undoGlobal(); pushToast("Undo"); }
            ImGui::SameLine();
            if (ImGui::Button(u8"\uf01e Redo", ImVec2(88, 26))) { viewer.getGraphManager().redoGlobal(); pushToast("Redo"); }
            ImGui::SameLine();
            if (ImGui::Button(u8"\uf0c7 Save", ImVec2(88, 26))) { viewer.getGraphManager().saveActive(); pushToast("Saved Active Graph"); }

            ImGui::Separator();
            ImGui::Checkbox("Draw Nodes", &viewer.getDebugDrawNodes());
            ImGui::SameLine();
            ImGui::Text("FPS %.1f", io.Framerate);
            ImGui::End();
            return;
        }

        ImGui::Separator();

        if (ImGui::BeginTabBar("DevDockTabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
            // ==== Actions ====
            if (ImGui::BeginTabItem(u8"\uf135 Actions")) {
                // Фиксированные колонки (не тянут кнопки при ресайзе)
                if (ImGui::BeginTable("ActionsGrid", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody)) {
                    auto Btn = [&](const char* label, const char* toast, auto fn) {
                        float w = ImGui::CalcTextSize(label).x + 24.0f;
                        if (ImGui::Button(label, ImVec2(w, 28))) { fn(); if (toast && *toast) pushToast(toast); }
                        };

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); Btn(u8"\uf0e2 Undo", "Undo", [&] { viewer.getGraphManager().undoGlobal(); });
                    ImGui::TableNextColumn(); Btn(u8"\uf01e Redo", "Redo", [&] { viewer.getGraphManager().redoGlobal(); });
                    ImGui::TableNextColumn(); Btn(u8"\uf0c7 Save Graph", "Saved Active Graph", [&] { viewer.getGraphManager().saveActive(); });

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); Btn(u8"\uf0c7 Save Trans", "Saved Transitions", [&] { viewer.getGraphManager().saveTransitions(Config::TRANSITIONS_PATH); });
                    ImGui::TableNextColumn(); Btn("Mark Savepoint", "Savepoint marked", [&] { viewer.getGraphManager().history.markSavepoint(); });
                    ImGui::TableNextColumn(); Btn("Dump History", "History dumped", [&] { viewer.getGraphManager().history.dumpToFile("history_dump.json"); });

                    ImGui::EndTable();
                }

                ImGui::Separator();
                bool clean = viewer.getGraphManager().history.isAtSavepoint();
                ImGui::Text("History: undo=%zu redo=%zu  |  %s",
                    viewer.getGraphManager().history.undoSize(),
                    viewer.getGraphManager().history.redoSize(),
                    clean ? "Saved" : "Modified");
                ImGui::EndTabItem();
            }

            // ==== Info ====
            if (ImGui::BeginTabItem(u8"\uf05a Info")) {
                const Camera& cam = viewer.getCamera();
                ImGui::Text("Zoom: %.0f%%", cam.getScale() * 100.0f);
                ImGui::Text("FPS:  %.1f", io.Framerate);
                ImGui::Text("Size: %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
                ImGui::Separator();

                if (ImGui::BeginTable("Toggles", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Checkbox("Draw Nodes", &viewer.getDebugDrawNodes());
                    ImGui::TableNextColumn(); ImGui::Checkbox("Allow Stairs [8]", &viewer.getUserAllowStairs());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Checkbox("Allow Lifts [9]", &viewer.getUserAllowLift());
                    ImGui::TableNextColumn(); ImGui::Checkbox("Allow Bridges [0]", &viewer.getUserAllowBridge());
                    ImGui::EndTable();
                }

                size_t cap = viewer.getGraphManager().history.getMaxEntries();
                int capInt = static_cast<int>(cap);
                ImGui::SliderInt("History Capacity", &capInt, 50, 1000, "%d");
                if (capInt != (int)cap) viewer.getGraphManager().history.setMaxEntries((size_t)capInt);

                ImGui::EndTabItem();
            }

            // ==== Inspector ====
            if (ImGui::BeginTabItem(u8"\uf002 Inspector")) {
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
                        if (ImGui::Button("Focus", ImVec2(100, 26))) viewer.focusNodeByIdSmart(n->id, true, false);
                        ImGui::SameLine();
                        if (ImGui::Button("Copy ID", ImVec2(100, 26))) ImGui::SetClipboardText(n->id.c_str());
                    }
                    else {
                        ImGui::TextDisabled("Node not found.");
                    }
                }
                ImGui::EndTabItem();
            }

            // ==== Goto ====
            if (ImGui::BeginTabItem(u8"\uf124 Goto")) {
                static std::string gotoStr;
                static bool optSwitchView = true;
                static bool optAutoZoom = false;
                static float targetZoom = 1.15f;

                ImGui::PushItemWidth(-1);
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                ImGui::InputTextWithHint("##gotoNode", "node id / alias", &gotoStr);
                ImGui::PopItemWidth();

                if (ImGui::Button("Go", ImVec2(60, 0))) {
                    viewer.focusNodeByIdSmart(gotoStr, optSwitchView, optAutoZoom, targetZoom);
                    pushToast("Focused");
                }

                ImGui::Checkbox("Switch view to node (campus/floor)", &optSwitchView);
                ImGui::Checkbox("Auto zoom", &optAutoZoom);
                if (optAutoZoom) ImGui::SliderFloat("Target zoom", &targetZoom, Config::MIN_ZOOM, Config::MAX_ZOOM, "%.2f");

                ImGui::TextDisabled("Hint: accepts alias or id. Switches view and focuses smoothly.");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void UIManager::pushToast(const std::string& txt, ImVec4 col, float duration) {
    toasts.push_back(Toast{ txt, col, duration, 0.0f });
}

void UIManager::drawToasts() {
    if (toasts.empty()) return;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    const float margin = 16.0f;
    ImVec2 pos(io.DisplaySize.x - margin, io.DisplaySize.y - margin); // старт снизу справа

    // Рисуем снизу вверх
    for (int i = (int)toasts.size() - 1; i >= 0; --i) {
        Toast& t = toasts[i];
        t.age += io.DeltaTime;
        float alpha = 1.0f;
        float fade = 0.25f; // последние 0.25с — fade-out
        if (t.age > t.ttl - fade) alpha = std::max(0.0f, (t.ttl - t.age) / fade);

        ImVec2 textSz = ImGui::CalcTextSize(t.text.c_str());
        ImVec2 boxSz(textSz.x + 20.0f, textSz.y + 12.0f);
        ImVec2 p1(pos.x - boxSz.x, pos.y - boxSz.y);
        ImVec2 p2(pos.x, pos.y);

        ImU32 bg = ImGui::GetColorU32(ImVec4(0.05f, 0.08f, 0.12f, 0.85f * alpha));
        ImU32 bd = ImGui::GetColorU32(ImVec4(0.20f, 0.40f, 0.90f, 0.9f * alpha));
        ImU32 fg = ImGui::GetColorU32(ImVec4(t.color.x, t.color.y, t.color.z, alpha));

        dl->AddRectFilled(p1, p2, bg, 8.0f);
        dl->AddRect(p1, p2, bd, 8.0f, 0, 1.5f);
        dl->AddText(ImVec2(p1.x + 10.0f, p1.y + 6.0f), fg, t.text.c_str());

        pos.y -= (boxSz.y + 8.0f);

        if (t.age >= t.ttl) {
            toasts.erase(toasts.begin() + i);
        }
    }
}