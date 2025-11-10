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
    static bool InputTextWithHint(const char* label, const char* hint,
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
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    // Адаптивные пропорции
    float headerH = Layout::HEADER_HEIGHT;
    float sidebarW = Layout::SIDEBAR_WIDTH * (screenW / 1920.0f);
    float rightbarW = std::clamp(Layout::RIGHTBAR_WIDTH * (screenW / 1920.0f), 280.0f, 360.0f);
    float margin = std::clamp(Layout::WINDOW_MARGIN * (screenW / 1920.0f), 16.0f, 32.0f);

    // --- Шапка ---
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screenW, headerH));
    drawTopNavBar(viewer);

    // --- Окно маршрута: адаптивный отступ и центрирование относительно Sidebar ---
    float routeX = sidebarW + margin;
    float routeY = headerH + margin * 1.2f;
    float routeW = std::clamp(430.0f * (screenW / 1920.0f), 320.0f, 480.0f);
    float routeH = std::clamp(240.0f * (screenH / 1080.0f), 200.0f, 280.0f);

    ImGui::SetNextWindowPos(ImVec2(routeX, routeY));
    ImGui::SetNextWindowSize(ImVec2(routeW, routeH));
    drawSearchWindow(viewer);

    drawFloorBuildingPanel(viewer);

    drawPlaceSearchWindow(viewer);

    drawBottomMenuBar(viewer);

   //  drawTransitionEffect(viewer);
#ifndef __EMSCRIPTEN__
    if (Config::DEV_MODE) {
        // Правая панель — всегда прижата к правому экранному краю
        float rightX = screenW - rightbarW - margin;
        float rightY = headerH + margin;
        float rightH = screenH - headerH - margin;

        ImGui::SetNextWindowPos(ImVec2(rightX, rightY));
        ImGui::SetNextWindowSize(ImVec2(rightbarW, rightH));
        drawRightPanel(viewer);
    }
#endif
}

void UIManager::drawSearchWindow(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    // --- геометрия окна ---
    const float WIDTH = 360.0f;
    const float HEIGHT = 230.0f;      // ↑ увеличено по Oy на ~10 %
    const float TOP = 20.0f;
    const float LEFT = 20.0f;
    const float RADIUS = 12.0f;

    ImGui::SetNextWindowPos(ImVec2(LEFT, TOP));
    ImGui::SetNextWindowSize(ImVec2(WIDTH, HEIGHT));
    ImGui::SetNextWindowBgAlpha(0.97f);

    if (ImGui::Begin("RoutePlannerCompact", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        // матовый фон + граница
        ImVec4 bg = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 border = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bg), RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(border), RADIUS, 0, 1.2f);

        ImGui::SetCursorPos(ImVec2(16, 14));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf14e  Построение маршрута");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 8));

        //-------------------------------------------------------
// поля ввода "Откуда" и "Куда"
//-------------------------------------------------------
        std::string& fromStr = mapViewer.getInputFrom();
        std::string& toStr = mapViewer.getInputTo();
        ImGui::PushItemWidth(-1);

        // --- поле ОТКУДА ---
        ImVec2 fromMin, fromMax;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.96f, 0.98f, 1.0f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.22f, 1.0f));
        if (ImGui::InputTextWithHint("##fromRoute", "Откуда...", &fromStr)) {
            mapViewer.setEditingFrom(true);    // запоминаем, что активно первое поле
            mapViewer.updateSuggestions();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        fromMin = ImGui::GetItemRectMin();
        fromMax = ImGui::GetItemRectMax();
        if (ImGui::IsItemActive())
            dl->AddRect(fromMin, fromMax, ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)),
                6.0f, 0, 2.0f);

        ImGui::Dummy(ImVec2(0, 8));

        // --- поле КУДА ---
        ImVec2 toMin, toMax;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.96f, 0.98f, 1.0f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.22f, 1.0f));
        if (ImGui::InputTextWithHint("##toRoute", "Куда...", &toStr)) {
            mapViewer.setEditingFrom(false);   // активное теперь второе поле
            mapViewer.updateSuggestions();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        toMin = ImGui::GetItemRectMin();
        toMax = ImGui::GetItemRectMax();
        if (ImGui::IsItemActive())
            dl->AddRect(toMin, toMax, ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.0f)),
                6.0f, 0, 2.0f);
        ImGui::PopItemWidth();

        // кнопка -------------------------------------------------------------
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.03f, 0.47f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.55f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.45f, 0.95f, 1.0f));
        ImGui::SetCursorPosX((WIDTH - 230.0f) * 0.5f);
        if (ImGui::Button("🚀  Построить маршрут", ImVec2(230.0f, 34.0f)))
            mapViewer.buildPathFromAliases(fromStr, toStr);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        //-------------------------------------------------------
        // отдельные popup‑подсказки (только для активного поля)
        //-------------------------------------------------------
        auto drawSuggestionsPopup = [&](const std::vector<std::string>& sugg,
            const ImVec2& fieldMin,
            const ImVec2& fieldMax,
            bool forFrom,
            std::string& fieldText)
            {
                if (sugg.empty()) return;

                // если введён текст полностью совпадает с одним из вариантов — скрываем список
                for (const std::string& s : sugg)
                    if (s == fieldText) return;

                const float popupW = 200.0f;
                const float popupShiftX = 14.0f;
                const float popupShiftY = forFrom ? 0.0f : 6.0f;
                const float RADIUS = 8.0f;
                std::string windowId = forFrom ? "##RouteSuggestionsFrom" : "##RouteSuggestionsTo";

                ImGui::SetNextWindowPos(ImVec2(fieldMax.x + popupShiftX, fieldMin.y + popupShiftY));
                ImGui::SetNextWindowSize(ImVec2(popupW, 130.0f));

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, RADIUS);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 0.97f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1.0f));

                bool open = true;
                ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoBringToFrontOnFocus;

                if (ImGui::Begin(windowId.c_str(), &open, flags))
                {
                    // фон и рамка MISIS‑стиля
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 pos = ImGui::GetWindowPos();
                    ImVec2 size = ImGui::GetWindowSize();
                    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                        ImGui::GetColorU32(ImVec4(1, 1, 1, 0.97f)), RADIUS);
                    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                        ImGui::GetColorU32(ImVec4(0.82f, 0.84f, 0.85f, 1)),
                        RADIUS, 0, 1.2f);

                    ImGui::Dummy(ImVec2(10, 6));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
                    ImGui::SetCursorPosX(12);
                    ImGui::TextUnformatted(u8"\uf002  Подсказки");
                    ImGui::PopStyleColor();
                    ImGui::Dummy(ImVec2(0, 4));

                    const float totalW = popupW - 20.0f;
                    ImGui::PushItemWidth(totalW);

                    for (size_t i = 0; i < sugg.size(); ++i) {
                        std::string id = sugg[i] + "##" + std::to_string(i) + windowId;
                        if (ImGui::Selectable(id.c_str(), false,
                            ImGuiSelectableFlags_None,
                            ImVec2(totalW, 26.0f)))
                        {
                            if (forFrom) {
                                mapViewer.getInputFrom() = sugg[i];
                                mapViewer.setEditingFrom(true);
                            }
                            else {
                                mapViewer.getInputTo() = sugg[i];
                                mapViewer.setEditingFrom(false);
                            }
                            open = false; // выбран вариант → закрываем
                        }
                    }

                    ImGui::PopItemWidth();

                    // закрываем при клике вне popup
                    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        open = false;
                }
                ImGui::End();

                // при закрытии возвращаем фокус к полю
                if (!open)
                    ImGui::SetKeyboardFocusHere(-1);

                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            };  //  ←‑‑‑ ЗАВЕРШЕНИЕ лямбды

        //-------------------------------------------------------
        // вызовы popup‑окон для активного поля
        //-------------------------------------------------------
        if (mapViewer.isEditingFrom())
            drawSuggestionsPopup(mapViewer.getCurrentSuggestions(), fromMin, fromMax, true, fromStr);
        else
            drawSuggestionsPopup(mapViewer.getCurrentSuggestions(), toMin, toMax, false, toStr);

        ImGui::End();
    }
}

// === Окно "Поиск места" (верхний правый угол) ===
void UIManager::drawPlaceSearchWindow(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    // Геометрия окна
    constexpr float WIDTH = 320.0f;
    constexpr float HEIGHT = 112.0f;   // −20% по Oy от 140
    constexpr float RADIUS = 12.0f;
    constexpr float TOP = 20.0f;
    constexpr float RIGHT = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(screenW - WIDTH - RIGHT, TOP));
    ImGui::SetNextWindowSize(ImVec2(WIDTH, HEIGHT));
    ImGui::SetNextWindowBgAlpha(0.97f);

    if (ImGui::Begin("FindPlaceCompact", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        // фон и рамка
        ImVec4 bgCol = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 borderCol = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bgCol), RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(borderCol), RADIUS, 0, 1.5f);

        // ===== Заголовок "Найти место" =====
        const ImVec4 textColor = ImVec4(0.05f, 0.25f, 0.65f, 1.0f);
        const float  ICON_SIZE = 18.0f;
        const float  LINE_H = 24.0f;

        ImGui::SetCursorPos(ImVec2(20, 16));
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        // Иконка маркера перед текстом
        ImGui::TextUnformatted(u8"\uf3c5");  // FontAwesome "map-marker-alt"
        ImVec2 iconMax = ImGui::GetItemRectMax();

        ImGui::SameLine();
        ImGui::SetCursorPosY(16.0f); // выравнивание текста по уровню иконки
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
        ImGui::TextUnformatted("Найти место");
        ImGui::PopStyleColor();

        // ===== Поле ввода =====
        static std::string query;
        ImGui::PushItemWidth(-22);
        ImGui::SetCursorPos(ImVec2(16, 46));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.96f, 0.98f, 1.0f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.20f, 1.0f));

        // рисуем иконку СЛЕВА во внутреннем отступе, чтобы текст начинался после неё
        ImVec2 fieldStart = ImGui::GetCursorScreenPos();
        ImVec2 iconPos = ImVec2(fieldStart.x + 10, fieldStart.y + 6);

        ImGui::SetCursorScreenPos(iconPos);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.52f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf002");
        ImGui::PopStyleColor();

        // отступ после иконки — чтобы текст не наезжал
        ImGui::SetCursorScreenPos(ImVec2(fieldStart.x + 28, fieldStart.y));

        bool submitted = ImGui::InputTextWithHint("##placeField",
            "Поиск аудиторий, кабинетов...", &query);

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        // Координаты поля
        ImVec2 a = ImGui::GetItemRectMin();
        ImVec2 b = ImGui::GetItemRectMax();

        // рамка при фокусе
        if (ImGui::IsItemActive())
            dl->AddRect(a, b,
                ImGui::GetColorU32(ImVec4(0.00f, 0.45f, 0.95f, 1.00f)),
                8.0f, 0, 2.0f);

        ImGui::PopItemWidth();
    }
    ImGui::End();
}

// === Info & Options ===
void UIManager::drawDevInfoWindow(MapViewer& mapViewer) {
    if (!ImGui::Begin("\uf129  Info & Options", nullptr,
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

// === Нижняя центральная панель навигации MISIS ===
void UIManager::drawBottomMenuBar(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    // Геометрия панели
    const float PANEL_H = 80.0f;
    const float PANEL_MARGIN_BOTTOM = 20.0f;
    const float PANEL_RADIUS = 16.0f;
    const float PANEL_W_SCALE = 0.9f; //если нудно длину по Ox поменять

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
    const float TEXT_HEIGHT = 18.0f;
    const float BLOCK_W = 100.0f;
    const float BLOCK_SPACING = 60.0f; // расстояние между блоками
    const float PANEL_W = (COUNT * BLOCK_W + (COUNT - 1) * BLOCK_SPACING);

    // Центрирование панели
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

        // === Фон панели ===
        ImVec4 bgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 borderColor = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bgColor), PANEL_RADIUS);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(borderColor), PANEL_RADIUS, 0, 1.5f);

        // Центральная координата панели
        float midY = pos.y + size.y * 0.5f;

        // цвета иконок
        ImVec4 defaultIconCol = ImVec4(0.06f, 0.40f, 0.95f, 1.0f);  // фирменный синий
        ImVec4 activeIconCol = ImVec4(0.00f, 0.60f, 1.0f, 1.0f);  // голубой ярче

        // расчёт позиций
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

            // === Иконка ===
            ImGui::SetCursorScreenPos(ImVec2(blockX - ICON_SIZE * 0.5f, iconY));
            ImGui::PushFont(io.Fonts->Fonts[0]);
            ImVec4 col = ImGui::IsMouseHoveringRect(ImVec2(blockX - 20, iconY - 4),
                ImVec2(blockX + 20, textY + 10))
                ? activeIconCol : defaultIconCol;
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Button(icon, ImVec2(ICON_SIZE, ICON_SIZE));
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // === Текст ===
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

// === Компактная верхняя панель MISIS — финальный вариант ===
void UIManager::drawTopNavBar(MapViewer& viewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    // Базовые параметры адаптивного окна
    constexpr float PANEL_H_BASE = 90.0f;
    constexpr float SHRINK_Y = 0.8f;
    constexpr float EXPAND_X = 1.2f;
    constexpr float TOP_MARGIN = 20.0f;
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

    // применяем масштаб
    float panelW = baseW * EXPAND_X;
    float panelH = PANEL_H_BASE * SHRINK_Y;
    float panelX = (screenW - panelW) * 0.5f;
    float panelY = TOP_MARGIN;

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

        // — матово‑белый фон и лёгкая серая рамка с округлёнными углами
        const float RADIUS = 12.0f;

        ImVec4 bgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 borderColor = ImVec4(0.82f, 0.84f, 0.85f, 1.0f);

        dl->AddRectFilled(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(bgColor), RADIUS
        );
        dl->AddRect(
            pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(borderColor), RADIUS, 0, 1.6f
        );

        // Центр панели по Y для вертикального выравнивания всех элементов
        float centerY = pos.y + size.y * 0.5f;

        // === Логотип MISIS ===
        if (logoTexture) {
            int tw, th;
            SDL_QueryTexture(logoTexture, nullptr, nullptr, &tw, &th);
            float scale = LOGO_MAX_H / static_cast<float>(th);
            ImVec2 logoSize(tw * scale, th * scale);

            // вертикальное центрирование изображения
            float logoY = centerY - logoSize.y * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 18.0f, logoY));
            ImGui::Image((ImTextureID)(intptr_t)logoTexture, logoSize);
        }
        else {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + 20.0f, centerY - 10.0f));
            ImGui::TextColored(ImVec4(0.0f, 0.35f, 0.75f, 1.0f), "MISIS");
        }

        // === Кнопки справа ===
        const float totalBtnW = (BTN_SIZE * 2.0f) + 14.0f; // две кнопки и отступ
        float btnStartX = pos.x + size.x - totalBtnW - 16.0f;
        float btnY = centerY - BTN_SIZE * 0.5f; // строго по центру панели

        ImGui::SetCursorScreenPos(ImVec2(btnStartX, btnY));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.96f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.91f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.86f, 1.0f, 1.0f));

        // 🌐 (FontAwesome \uf1ab) — переключение языка
        const char* LANG_ICON = u8"\uf1ab";
        const char* FULL_ICON = u8"\uf065"; // ⛶ fullscreen

        if (ImGui::Button(LANG_ICON, ImVec2(BTN_SIZE, BTN_SIZE))) {
            SDL_Log("[UI] Language switch clicked");
        }
        // «приподнятие» при наведении
        if (ImGui::IsItemHovered()) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            dl->AddRect(min, max,
                ImGui::GetColorU32(ImVec4(0.18f, 0.45f, 1.0f, 0.55f)),
                10.0f, 0, 2.0f);
            // лёгкий offset вверх
            ImGui::SetCursorScreenPos(ImVec2(min.x, min.y - 1.0f));
        }

        ImGui::SameLine(0, 14.0f);

        if (ImGui::Button(FULL_ICON, ImVec2(BTN_SIZE, BTN_SIZE))) {
            SDL_Window* win = SDL_GL_GetCurrentWindow();
            Uint32 flags = SDL_GetWindowFlags(win);
            SDL_SetWindowFullscreen(win,
                (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        if (ImGui::IsItemHovered())
            dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                ImGui::GetColorU32(ImVec4(0.20f, 0.50f, 1.0f, 0.3f)), 10.0f, 0, 1.5f);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
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

    static float panelWidth = std::clamp(screenW * 0.22f, 280.0f, 360.0f);
#ifndef __EMSCRIPTEN__
    // В DEV-режиме разрешаем "resize" ширины, с лимитами
    if (Config::DEV_MODE) {
        float newWidth = panelWidth;
        ImGui::SetNextWindowPos(ImVec2(screenW - panelWidth, HEADER_HEIGHT));
        ImGui::SetNextWindowSize(ImVec2(panelWidth, screenH - HEADER_HEIGHT));
        if (ImGui::Begin("\uf12e Right Panel (Dev)", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoTitleBar))
        {

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.97f)), 10.0f);
            dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                ImGui::GetColorU32(ImVec4(0, 0, 0, 0.07f)), 10.0f, 0, 2.0f);

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

// === Вертикальный блок выбора вида, корпуса и этажей ===
void UIManager::drawFloorBuildingPanel(MapViewer& mapViewer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    //---------------------------------------------
    // Геометрия окна — под окном "Найти место"
    //---------------------------------------------
    constexpr float PANEL_W = 260.0f;
    constexpr float BUTTON_H = 28.0f;  // более компактные кнопки этажей
    constexpr float MARGIN_RIGHT = 20.0f;
    constexpr float TOP_OFFSET = 150.0f;  // расстояние от верха после "Найти место"
    constexpr float GAP = 6.0f;
    constexpr float RADIUS = 12.0f;

    // вычисляем количество этажей для динамической высоты
    int floorCount = 0;
    {
        const BuildingMeta* meta = mapViewer.getGraphManager()
            .getBuildingMeta(mapViewer.getCurrentBuilding());
        if (meta) floorCount = static_cast<int>(meta->floors.size());
    }
    float PANEL_H = 120.0f + floorCount * (BUTTON_H + GAP);

    float panelX = screenW - PANEL_W - MARGIN_RIGHT;
    float panelY = TOP_OFFSET;

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

        ImGui::SetCursorPos(ImVec2(16, 14));

        //---------------------------------------------
        // --- Строка "Выбор корпуса" + иконка здания
        //---------------------------------------------
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
        ImGui::TextUnformatted(u8"\uf1ad"); // иконка здания (FontAwesome)
        ImGui::SameLine(28);
        ImGui::TextUnformatted("Корпус");
        ImGui::PopStyleColor();

        // выпадающий список корпусов
        const auto& metas = mapViewer.getGraphManager().buildingMetas;
        std::string currentBuilding = mapViewer.getCurrentBuilding();

        const char* currentLabel =
            currentBuilding.empty() ? "— выберите корпус —" : currentBuilding.c_str();

        static bool popupOpen = false;
        ImVec2 comboMin{}, comboMax{};

        ImGui::SetCursorPos(ImVec2(16, 42));
        ImGui::PushItemWidth(PANEL_W - 32);

        // --- старый BeginCombo для фикса: имитируем поведение, но логируем детально ---
        if (ImGui::BeginCombo("##buildingSelectDebug", currentLabel)) {
            std::cout << "[UI][DEBUG] ИмGui BeginCombo открыл базовое popup." << std::endl;
            popupOpen = true;
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        // координаты строки выбора корпуса
        comboMin = ImGui::GetItemRectMin();
        comboMax = ImGui::GetItemRectMax();

        // лог текущего состояния
        std::cout << "[UI][DEBUG] Cursor after combo. popupOpen=" << popupOpen
            << " rect: (" << comboMin.x << "," << comboMin.y
            << ")→(" << comboMax.x << "," << comboMax.y << ")" << std::endl;

        if (popupOpen) {
            const float popupW = 200.0f;
            const float popupH = 240.0f;
            const float popupShiftX = 25.0f;   // немного ближе к окну
            const float popupShiftY = 0.0f;
            const float CORNER_RADIUS = 8.0f;

            ImGui::SetNextWindowPos(ImVec2(comboMin.x - popupW - popupShiftX,
                comboMin.y + popupShiftY));
            ImGui::SetNextWindowSize(ImVec2(popupW, popupH));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, CORNER_RADIUS);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 0.97f)); // фон в стиле основного окна
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.82f, 0.84f, 0.85f, 1.0f));

            if (ImGui::Begin("##BuildingListPopupFinal", &popupOpen,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar))
            {
                // аккуратный верхний отступ
                ImGui::Dummy(ImVec2(10, 6));

                // иконка здания + заголовок
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.25f, 0.65f, 1.0f));
                ImGui::SetCursorPosX(12);
                ImGui::TextUnformatted(u8"\uf1ad");
                ImGui::SameLine(30);
                ImGui::TextUnformatted("Выберите корпус");
                ImGui::PopStyleColor();

                // отступ до списка
                ImGui::Dummy(ImVec2(0, 8));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 6));

                // задаём ширину элементов на всю ширину окна
                const float totalW = popupW - 20.0f;
                ImGui::PushItemWidth(totalW);

                // список корпусов
                for (const auto& [bid, meta] : metas) {
                    bool selected = (bid == currentBuilding);
                    ImGui::SetCursorPosX(10); // немного ближе к левой границе
                    if (ImGui::Selectable(meta.name.c_str(), selected,
                        ImGuiSelectableFlags_None,
                        ImVec2(totalW, 28))) {
                        if (!meta.floors.empty())
                            mapViewer.switchToFloor(bid, meta.floors.front().floor);
                        popupOpen = false;
                    }
                }

                ImGui::Dummy(ImVec2(0, 4));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 4));

                // кнопка "вид сверху (кампус)" – выделена приятным голубым оттенком, со скруглением
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, CORNER_RADIUS);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.93f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.90f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.87f, 1.0f, 1.0f));

                ImGui::SetCursorPosX(10);
                if (ImGui::Button("🏫 Вид сверху (кампус)", ImVec2(totalW, 30.0f))) {
                    mapViewer.switchViewToCampus();
                    popupOpen = false;
                }

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
            }
            ImGui::End();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            // закрываем при клике вне окна
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                popupOpen = false;
        }

        // клик по строке выбора корпуса открывает/закрывает popup
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            popupOpen = !popupOpen;

        //---------------------------------------------
        // --- Надпись "Этажи"
        //---------------------------------------------
        const BuildingMeta* bm =
            mapViewer.getGraphManager().getBuildingMeta(currentBuilding);
        if (bm && !bm->floors.empty()) {
            ImGui::SetCursorPos(ImVec2(16, 80));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.18f, 0.26f, 1.0f));
            ImGui::TextUnformatted("Этажи:");
            ImGui::PopStyleColor();

            std::vector<int> floors;
            for (const auto& f : bm->floors)
                floors.push_back(f.floor);
            std::sort(floors.begin(), floors.end(), std::greater<int>());

            // координата для начала перечисления
            float y = 100.0f;
            for (int f : floors) {
                ImGui::SetCursorPos(ImVec2(24, y));
                bool active = (f == mapViewer.getCurrentFloor());
                ImVec4 col = active
                    ? ImVec4(0.08f, 0.50f, 1.0f, 1.0f)
                    : ImVec4(1.0f, 1.0f, 1.0f, 0.95f);
                ImGui::PushStyleColor(ImGuiCol_Button, col);
                std::string label = "Этаж " + std::to_string(f);
                if (ImGui::Button(label.c_str(),
                    ImVec2(PANEL_W - 48, BUTTON_H))) {
                    mapViewer.switchToFloor(currentBuilding, f);
                }
                ImGui::PopStyleColor();
                y += BUTTON_H + GAP;
            }
        }
        else {
            ImGui::SetCursorPos(ImVec2(16, 90));
            ImGui::TextDisabled("Этажи отсутствуют");
        }

    }
    ImGui::End();
}