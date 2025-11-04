#include "UIManager.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../config.hpp"
#include <iostream>
#include <string>

namespace Layout {
    constexpr float HEADER_HEIGHT = 60.0f;
    constexpr float SIDEBAR_WIDTH = 260.0f;
    constexpr float RIGHTBAR_WIDTH = 340.0f;
    constexpr float WINDOW_MARGIN = 20.0f;
}

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
    bool InputTextWithHint(const char* label, const char* hint,
        std::string* str, ImGuiInputTextFlags flags = 0) {
        flags |= ImGuiInputTextFlags_CallbackResize;
        return ImGui::InputTextWithHint(
            label, hint, str->data(), str->capacity() + 1,
            flags, InputTextCallback, (void*)str);
    }
}

// --- Конструктор ---
UIManager::UIManager() {}

// --- render() ---
void UIManager::render(MapViewer& viewer) {
    // --- Шапка ---
    drawTopNavBar(viewer);

    // --- Левая панель навигации ("избранное" или меню) ---
    ImGui::SetNextWindowPos(ImVec2(0, Layout::HEADER_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(Layout::SIDEBAR_WIDTH, Config::INITIAL_WINDOW_HEIGHT - Layout::HEADER_HEIGHT));
    drawLeftSidebar(viewer);  // сделаем на следующем шаге

    // --- Основное окно поиска маршрута ---
    float routePanelX = Layout::SIDEBAR_WIDTH + Layout::WINDOW_MARGIN;
    float routePanelY = Layout::HEADER_HEIGHT + Layout::WINDOW_MARGIN;
    ImGui::SetNextWindowPos(ImVec2(routePanelX, routePanelY));
    ImGui::SetNextWindowSize(ImVec2(420, 230));
    drawSearchWindow(viewer);

#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        float rightX = Config::INITIAL_WINDOW_WIDTH - Layout::RIGHTBAR_WIDTH - Layout::WINDOW_MARGIN;
        float rightY = Layout::HEADER_HEIGHT + Layout::WINDOW_MARGIN;
        ImGui::SetNextWindowPos(ImVec2(rightX, rightY));
        ImGui::SetNextWindowSize(ImVec2(Layout::RIGHTBAR_WIDTH, 280));

        ImGui::SetNextWindowPos(ImVec2(rightX,
            rightY + 280 + Layout::WINDOW_MARGIN));
        ImGui::SetNextWindowSize(ImVec2(Layout::RIGHTBAR_WIDTH, 260));
        drawRightPanel(viewer);
    }
#endif
}

void UIManager::drawSearchWindow(MapViewer& mapViewer) {
    // фиксированная панель без рамки/заголовка
    constexpr float FIELD_HEIGHT = 28.0f;

    if (ImGui::Begin("Route Planner", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar))
    {
        std::string& fromStr = mapViewer.getInputFrom();
        std::string& toStr = mapViewer.getInputTo();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Campus Route Finder");
        ImGui::Separator();
        ImGui::PushItemWidth(-1);

        // --- Ввод "From" ---
        ImGui::InputTextWithHint("##from", "From location...", &fromStr);
        if (ImGui::IsItemActivated()) mapViewer.setEditingFrom(true);

        // --- Ввод "To" ---
        ImGui::InputTextWithHint("##to", "To location...", &toStr);
        if (ImGui::IsItemActivated()) mapViewer.setEditingFrom(false);

        // --- Кнопка ---
        ImGui::Spacing();
        if (ImGui::Button("🚀  Build Path", ImVec2(-1, FIELD_HEIGHT))) {
            mapViewer.buildPathFromAliases(fromStr, toStr);
        }

        // --- Подсказки ---
        const auto& suggestions = mapViewer.getCurrentSuggestions();
        if (!suggestions.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Suggestions:");
            ImGui::BeginChild("suggestions", ImVec2(0, 100), true);
            for (const auto& s : suggestions) {
                if (ImGui::Selectable(s.c_str())) {
                    if (mapViewer.isEditingFrom()) fromStr = s;
                    else                           toStr = s;
                    mapViewer.clearSuggestions();
                    break;
                }
            }
            ImGui::EndChild();
        }
        ImGui::PopItemWidth();
    }
    ImGui::End();
}


// === Info & Options ===
void UIManager::drawDevInfoWindow(MapViewer& mapViewer) {
    if (!ImGui::Begin("ℹ️  Info & Options", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    const Camera& camera = mapViewer.getCamera();
    const SDL_Point& mouseWorld = mapViewer.getDebugMouseWorld();

    // --- Раздел «Debug Info» ---
    ImGui::SeparatorText("Debug Info");
    ImGui::Text("Zoom: %.0f%%", camera.getScale() * 100.0f);
    ImGui::Text("Mouse World: X=%d  Y=%d", mouseWorld.x, mouseWorld.y);
    ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

    // --- Статусы / сообщения ---
    if (mapViewer.isNeighborModeActive()) {
        ImGui::SeparatorText("Status");
        ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f),
            "Neighbor Mode for: %s",
            mapViewer.getNeighborModeActiveId().c_str());
    }

    Uint32 lastSaveTick = mapViewer.getLastSaveTick();
    if (lastSaveTick != 0 && SDL_GetTicks() - lastSaveTick < 2000) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Graph Saved!");
    }

    // --- Настройки пути ---
    ImGui::SeparatorText("Pathfinding Options");
    ImGui::Checkbox("Allow Stairs [8]", &mapViewer.getUserAllowStairs());
    ImGui::Checkbox("Allow Lifts [9]", &mapViewer.getUserAllowLift());
    ImGui::Checkbox("Allow Bridges [0]", &mapViewer.getUserAllowBridge());

    ImGui::End();
}


// === Node Inspector ===
// === Верхняя панель навигации (чистый визуальный слой) ===
void UIManager::drawTopNavBar(MapViewer& viewer) {
    using namespace Layout;
    constexpr float HEADER_H = HEADER_HEIGHT;
    constexpr float BUTTON_W = 110.0f;
    constexpr float ITEM_GAP = 16.0f;

    ImGuiIO& io = ImGui::GetIO();                      // чтобы знать реальный размер окна
    float screenW = io.DisplaySize.x;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screenW, HEADER_H), ImGuiCond_Always);

    if (ImGui::Begin("TopNavBar", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar))
    {
        // Градиентный фон
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        ImU32 c1 = ImGui::GetColorU32(ImVec4(0.08f, 0.22f, 0.45f, 1.0f));
        ImU32 c2 = ImGui::GetColorU32(ImVec4(0.10f, 0.26f, 0.55f, 1.0f));
        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            c1, c1, c2, c2);

        // ==== ЛОГО ====
        ImGui::SetCursorPos(ImVec2(18, 14));
        ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f),
            "🏫  MISIS Campus Map 2025");

        // ==== ПОЛЕ ПОИСКА ====
        float leftStart = SIDEBAR_WIDTH + ITEM_GAP * 2;
        float availableW = screenW - SIDEBAR_WIDTH - RIGHTBAR_WIDTH
            - BUTTON_W * 2 - ITEM_GAP * 6;
        ImGui::SetCursorPos(ImVec2(leftStart, 12));
        ImGui::PushItemWidth(availableW);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));

        static std::string query;
        ImGui::InputTextWithHint("##SearchTopHint",
            "Search for place or room...",
            &query);

        /*if (!query.empty() && query.size() > 1) {
            auto suggestions = viewer.getAliasManager().suggest(query, 6);
            if (!suggestions.empty()) {
                ImGui::SetNextWindowPos(ImVec2(Layout::SIDEBAR_WIDTH + 40, Layout::HEADER_HEIGHT + 4));
                ImGui::SetNextWindowSize(ImVec2(420, suggestions.size() * 28 + 14));
                if (ImGui::Begin("SearchSuggest", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
                {
                    for (auto& s : suggestions) {
                        if (ImGui::Selectable(s.c_str())) {
                            viewer.getInputFrom() = s;
                            viewer.clearSuggestions();
                            query.clear();
                            break;
                        }
                    }
                }
                ImGui::End();
            }
        }
        */

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // ==== КНОПКИ СПРАВА ====
        float btnY = 12.0f;
        float btnX = screenW - RIGHTBAR_WIDTH - BUTTON_W * 2 - ITEM_GAP * 3;
        ImGui::SetCursorPos(ImVec2(btnX, btnY));

        if (ImGui::Button("🌐 RU/EN", ImVec2(BUTTON_W, HEADER_H - 24))) {
            std::cout << "[UI] Language toggle clicked\n";
        }
        ImGui::SameLine();
        if (ImGui::Button("⛶ Fullscreen", ImVec2(BUTTON_W, HEADER_H - 24))) {
            SDL_Window* win = SDL_GL_GetCurrentWindow();
            Uint32 flags = SDL_GetWindowFlags(win);
            bool isFull = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
            SDL_SetWindowFullscreen(win, isFull ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        // ==== Разделительная линия (тень) ====
        dl->AddLine(ImVec2(pos.x, pos.y + size.y - 1),
            ImVec2(pos.x + size.x, pos.y + size.y - 1),
            ImGui::GetColorU32(ImVec4(0, 0, 0, 0.35f)), 1.0f);

    }
    ImGui::End();
}

// === Левая боковая панель (Sidebar / Избранное) ===
void UIManager::drawLeftSidebar(MapViewer& viewer) {
    static bool collapsed = false;     // состояние – свернута или нет
    static int  selected = -1;        // выбранный пункт
    constexpr float ITEM_HEIGHT = 38.0f;
    constexpr float INDICATOR_WIDTH = 4.0f;

    float sidebarWidth = collapsed ? 70.0f : Layout::SIDEBAR_WIDTH; // узкая в свернутом виде
    ImGui::SetNextWindowPos(ImVec2(0, Layout::HEADER_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth,
        Config::INITIAL_WINDOW_HEIGHT - Layout::HEADER_HEIGHT));

    if (ImGui::Begin("📚 Sidebar", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar))
    {
        // Кнопка-гамбургер свернуть/развернуть
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
        if (ImGui::Button(collapsed ? "☰" : "☰  Menu", ImVec2(-1, 40))) {
            collapsed = !collapsed;
        }
        ImGui::PopStyleVar();
        ImGui::Separator();

        // Список избранных
        static const std::pair<const char*, const char*> items[] = {
            { "📚", "Library" },
            { "🍽", "Cafeteria" },
            { "🏋️", "Gym" },
            { "🧭", "Main Square" },
            { "🏢", "Administrative" }
        };

        ImVec2 cursorStart = ImGui::GetCursorScreenPos();
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            // Вычисляем состояние
            bool hovered = false;
            bool clicked = false;

            // позиция кнопки
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, ITEM_HEIGHT);

            // фон при наведении / выборе
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImU32 bgColor = 0;
            if (selected == i)
                bgColor = ImGui::GetColorU32(ImVec4(0.18f, 0.44f, 0.85f, 1.0f));
            else if (ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y)))
                bgColor = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.30f, 1.0f));

            if (bgColor)
                dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, 6.0f);

            // активный индикатор
            if (selected == i) {
                dl->AddRectFilled(ImVec2(pos.x, pos.y),
                    ImVec2(pos.x + INDICATOR_WIDTH, pos.y + size.y),
                    ImGui::GetColorU32(ImVec4(0.9f, 0.6f, 0.2f, 1.0f)));
            }

            // Содержимое строки
            ImGui::InvisibleButton(("##item" + std::to_string(i)).c_str(), size);
            if (ImGui::IsItemHovered())
                hovered = true;
            if (ImGui::IsItemClicked())
                clicked = true;

            ImVec2 textPos = ImVec2(pos.x + 12.0f + (collapsed ? 0.0f : INDICATOR_WIDTH),
                pos.y + 8.0f);

            dl->AddText(textPos, ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]),
                items[i].first);

            if (!collapsed) {
                dl->AddText(ImVec2(textPos.x + 26.0f, pos.y + 8.0f),
                    ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]),
                    items[i].second);
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + ITEM_HEIGHT + 6));

            // реакция на клик
            if (clicked) {
                selected = i;
                viewer.getInputFrom() = items[i].second; // заполняем поле поиска
                viewer.updateSuggestions();
            }
        }

        // Низ панели
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::SeparatorText("System");

        if (ImGui::Button("💾  Save", ImVec2(-1, 0))) {
            std::cout << "Data save\n";
        }
        if (ImGui::Button("🔄  Reload", ImVec2(-1, 0))) {
            std::cout << "Reload requested\n";
        }

    }
    ImGui::End();
}

// === Правая мультивкладочная панель (Info / Inspector / Console) ===
void UIManager::drawRightPanel(MapViewer& viewer) {
    using namespace Layout;
    constexpr float MIN_PANEL_WIDTH = 320.0f;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    static float panelWidth = RIGHTBAR_WIDTH;
#ifndef __EMSCRIPTEN__
    // В DEV-режиме разрешаем "resize" ширины, с лимитами
    if (Config::DEV_MODE) {
        float newWidth = panelWidth;
        ImGui::SetNextWindowPos(ImVec2(screenW - panelWidth, HEADER_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(panelWidth, screenH - HEADER_HEIGHT));
        if (ImGui::Begin("🧩 Right Panel (Dev)", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar))
        {

            if (fabs(newWidth - panelWidth) > 1.0f)
                panelWidth = newWidth;

            ImGui::Dummy(ImVec2(0, 6));
            if (ImGui::BeginTabBar("RightTabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
                // ==== Info ====
                if (ImGui::BeginTabItem("ℹ️ Info")) {
                    const Camera& cam = viewer.getCamera();
                    const SDL_Point& pMouse = viewer.getDebugMouseWorld();
                    ImGui::Text("Zoom %.0f%%", cam.getScale() * 100.f);
                    ImGui::Text("Mouse: (%d,%d)", pMouse.x, pMouse.y);
                    ImGui::EndTabItem();
                }
                // ==== Inspector ====
                if (ImGui::BeginTabItem("🔍 Inspector")) {
                    const std::string& nodeId = viewer.getInspectorNodeId();
                    if (!nodeId.empty()) {
                        const Node* n = viewer.getGraphManager().getNode(nodeId);
                        if (n)
                            ImGui::Text("Node: %s  [floor %d]", n->id.c_str(), n->floor);
                    }
                    else ImGui::TextDisabled("No node selected.");
                    ImGui::EndTabItem();
                }
                // ==== Dev Console ====
                if (ImGui::BeginTabItem("🛠️ Console")) {
                    static char consoleBuf[512] = "";
                    static std::vector<std::string> history;

                    ImGui::BeginChild("ConsoleLog",
                        ImVec2(-1, ImGui::GetContentRegionAvail().y - 40),
                        true);
                    for (auto& line : history)
                        ImGui::TextUnformatted(line.c_str());
                    ImGui::EndChild();

                    ImGui::Separator();
                    if (ImGui::InputText("##cmd", consoleBuf,
                        IM_ARRAYSIZE(consoleBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        std::string cmd = consoleBuf;
                        history.push_back("> " + cmd);
                        if (cmd == "clear") history.clear();
                        else if (cmd == "help")
                            history.push_back("available: clear, help, reload, save");
                        else if (cmd == "reload")
                            history.push_back("[system] reload requested");
                        else if (cmd == "save")
                            history.push_back("[system] save requested");
                        else history.push_back("[echo] " + cmd);

                        *consoleBuf = 0;
                        ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    else
#endif
    {
        // --- USER режим (Web и обычный пользователь)
        float panelX = screenW - RIGHTBAR_WIDTH;
        float panelY = HEADER_HEIGHT;
        float panelW = RIGHTBAR_WIDTH;
        float panelH = screenH - HEADER_HEIGHT;

        ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

        if (ImGui::Begin("🧩 Right Panel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar))
        {
            if (ImGui::BeginTabBar("RightTabsUser", ImGuiTabBarFlags_FittingPolicyResizeDown)) {

                if (ImGui::BeginTabItem("ℹ️ Info")) {
                    const Camera& cam = viewer.getCamera();
                    const SDL_Point& pMouse = viewer.getDebugMouseWorld();
                    ImGui::Text("Zoom %.0f%%", cam.getScale() * 100.f);
                    ImGui::Text("Mouse: (%d,%d)", pMouse.x, pMouse.y);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("🔍 Inspector")) {
                    const std::string& id = viewer.getInspectorNodeId();
                    if (id.empty())
                        ImGui::TextDisabled("No node selected");
                    else {
                        const Node* n = viewer.getGraphManager().getNode(id);
                        if (n)
                            ImGui::Text("ID: %s  (%s)", n->id.c_str(), n->building.c_str());
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
}