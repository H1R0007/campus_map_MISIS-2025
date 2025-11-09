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

    // --- Левая панель (адаптивная по высоте) ---
    float sidebarH = screenH - headerH;
    ImGui::SetNextWindowPos(ImVec2(0, headerH));
    ImGui::SetNextWindowSize(ImVec2(sidebarW, sidebarH));
    drawLeftSidebar(viewer);

    // --- Окно маршрута: адаптивный отступ и центрирование относительно Sidebar ---
    float routeX = sidebarW + margin;
    float routeY = headerH + margin * 1.2f;
    float routeW = std::clamp(430.0f * (screenW / 1920.0f), 320.0f, 480.0f);
    float routeH = std::clamp(240.0f * (screenH / 1080.0f), 200.0f, 280.0f);

    ImGui::SetNextWindowPos(ImVec2(routeX, routeY));
    ImGui::SetNextWindowSize(ImVec2(routeW, routeH));
    drawSearchWindow(viewer);

    drawFloorBuildingPanel(viewer);

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
    constexpr float FIELD_HEIGHT = 34.0f;
    constexpr float CARD_ALPHA = 0.97f;
    constexpr float CORNER_RAD = 12.0f;
    constexpr float EDGE_GLOW_STRENGTH = 0.25f;

    // Позиция и размеры окна теперь аккуратнее по ширине, выше по высоте
    float screenW = ImGui::GetIO().DisplaySize.x;
    float screenH = ImGui::GetIO().DisplaySize.y;

    float routeW = std::clamp(400.0f * (screenW / 1920.0f), 360.0f, 440.0f);
    float routeH = std::clamp(260.0f * (screenH / 1080.0f), 230.0f, 320.0f);
    float topOffset = Layout::HEADER_HEIGHT + 6.0f;
    float rightMargin = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(screenW - routeW - rightMargin, topOffset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(routeW, routeH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(CARD_ALPHA);

    if (ImGui::Begin("\uf14e Route Planner", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar))
    {
        //------------------------------------------------------------------
        // 🔲 Подложка панели: более плавное и медленное "дыхание"
        //------------------------------------------------------------------
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        float t = SDL_GetTicks() * 0.0005f;           // медленнее в 4 раза
        float osc = 0.5f + 0.5f * sinf(t * 1.1f);     // мягкое дыхание

        ImVec4 topColor = ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
        ImVec4 bottomColor = ImVec4(0.82f + 0.10f * osc,
            0.88f + 0.07f * osc,
            1.0f,
            1.0f);
        ImU32 cTop = ImGui::GetColorU32(topColor);
        ImU32 cBottom = ImGui::GetColorU32(bottomColor);

        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            cTop, cTop, cBottom, cBottom);

        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(ImVec4(0, 0, 0, 0.10f)), CORNER_RAD, 0, 2.0f);
        //------------------------------------------------------------------
        // ✨ Glowing Edges — свечение только по краям при наведении курсора
        //------------------------------------------------------------------

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool hovered = (mousePos.x >= pos.x && mousePos.x <= pos.x + size.x &&
            mousePos.y >= pos.y && mousePos.y <= pos.y + size.y);

        if (hovered) {
            // --- 1. Временной коэффициент для анимации
            float time = SDL_GetTicks() * 0.0015f; // 1.5 цикла в секунду

            // --- 2. Определяем динамику пульса и волны
            float pulse = 0.5f + 0.5f * sinf(time * 2.5f);
            float wave = sinf(time * 3.5f) * 0.15f;

            // --- 3. Глобальный цветовой сдвиг между голубым и лазурным
            ImVec4 baseCol = ImVec4(0.05f + wave, 0.45f + pulse * 0.25f, 1.0f, 1.0f);
            ImVec4 outerGlow = ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.55f + pulse * 0.25f);
            ImVec4 midGlow = ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.35f + pulse * 0.20f);
            ImVec4 innerGlow = ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.18f + pulse * 0.12f);

            // --- 4. Конфигурация уровней свечения
            const int layers = 6;
            const float spacing = 1.8f; // расстояние между слоями

            // --- 5. Отрисовываем концентрические рамки (наружу ярче и немного шире)
            for (int i = 0; i < layers; ++i) {
                float expand = i * spacing;
                ImVec4 colStep;
                float alphaStep = 1.0f - (i / float(layers));

                // переключаем плавно от яркого к мягкому оттенку
                if (i < 2)       colStep = outerGlow;
                else if (i < 4)  colStep = midGlow;
                else              colStep = innerGlow;

                colStep.w *= alphaStep;
                dl->AddRect(ImVec2(pos.x - expand, pos.y - expand),
                    ImVec2(pos.x + size.x + expand, pos.y + size.y + expand),
                    ImGui::GetColorU32(colStep),
                    CORNER_RAD + expand,
                    0,
                    3.0f);
            }

            // --- 6. Добавляем рассеянное внутреннее свечение по углам (не резкий отблеск)
            ImU32 cornerColor = ImGui::GetColorU32(ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.25f + pulse * 0.25f));

            // верхний и нижний края лёгким градиентом в стороны
            for (int i = 0; i < 5; ++i) {
                float fade = (1.0f - i / 5.0f) * (0.5f + pulse * 0.5f);
                float offset = 3.0f + i * 1.5f;
                ImU32 c = ImGui::GetColorU32(ImVec4(baseCol.x, baseCol.y, baseCol.z, 0.15f * fade));
                // верх
                dl->AddLine(ImVec2(pos.x + offset, pos.y - offset),
                    ImVec2(pos.x + size.x - offset, pos.y - offset), c, 2.0f + i * 0.5f);
                // низ
                dl->AddLine(ImVec2(pos.x + offset, pos.y + size.y + offset),
                    ImVec2(pos.x + size.x - offset, pos.y + size.y + offset), c, 2.0f + i * 0.5f);
                // лево
                dl->AddLine(ImVec2(pos.x - offset, pos.y + offset),
                    ImVec2(pos.x - offset, pos.y + size.y - offset), c, 2.0f + i * 0.5f);
                // право
                dl->AddLine(ImVec2(pos.x + size.x + offset, pos.y + offset),
                    ImVec2(pos.x + size.x + offset, pos.y + size.y - offset), c, 2.0f + i * 0.5f);
            }

            // --- 7. Эффект "движущейся волны" по периметру (немного света, едет по кругу)
            const int segs = 60;
            float perimeter = 2.0f * (size.x + size.y);
            float offsetAnim = fmodf(time * 120.0f, perimeter);

            ImVec4 waveColor = ImVec4(0.20f * pulse, 0.55f + 0.3f * pulse, 1.0f, 0.8f);
            float lengthAnim = 120.0f + 60.0f * pulse;

            // создаем 4 сегмента по сторонам
            auto drawMovingEdge = [&](ImVec2 a, ImVec2 b, float length, float offset, ImVec4 color) {
                ImVec2 dir = { b.x - a.x, b.y - a.y };
                float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if (len < 1.0f) return;
                dir.x /= len; dir.y /= len;

                float start = fmodf(offset, len);
                float end = start + length;
                if (end > len) end = len;

                ImVec2 p1 = { a.x + dir.x * start, a.y + dir.y * start };
                ImVec2 p2 = { a.x + dir.x * end,   a.y + dir.y * end };
                dl->AddLine(p1, p2, ImGui::GetColorU32(color), 4.0f);
                };

            float perimeterLength = (size.x + size.y) * 2;
            float animOffset = fmodf(time * 250.0f, perimeterLength);

            // четыре стороны – двигающаяся волна цвета
            drawMovingEdge(ImVec2(pos.x, pos.y),
                ImVec2(pos.x + size.x, pos.y),
                lengthAnim, animOffset, waveColor);
            drawMovingEdge(ImVec2(pos.x + size.x, pos.y),
                ImVec2(pos.x + size.x, pos.y + size.y),
                lengthAnim, animOffset - size.x, waveColor);
            drawMovingEdge(ImVec2(pos.x + size.x, pos.y + size.y),
                ImVec2(pos.x, pos.y + size.y),
                lengthAnim, animOffset - (size.x + size.y), waveColor);
            drawMovingEdge(ImVec2(pos.x, pos.y + size.y),
                ImVec2(pos.x, pos.y),
                lengthAnim, animOffset - (2 * size.x + size.y), waveColor);
        }
        //------------------------------------------------------------------
        // 🪄 Заголовок
        //------------------------------------------------------------------
        ImGui::PushItemWidth(-1);
        ImGui::TextColored(ImVec4(0.0f, 0.25f, 0.65f, 1.0f), "Построение маршрута");
        ImGui::Separator();
        ImGui::Spacing();

        //------------------------------------------------------------------
        // ✍️ Поля ввода — аккуратные, без подсветки, с ясной активной заливкой
        //------------------------------------------------------------------
        std::string& fromStr = mapViewer.getInputFrom();
        std::string& toStr = mapViewer.getInputTo();
        const char* hints[2] = { "Откуда...", "Куда..." };
        std::string* strs[2] = { &fromStr, &toStr };

        ImGui::PushItemWidth(-1.0f);

        for (int i = 0; i < 2; ++i)
        {
            // --- ввод ---
            bool isFrom = (i == 0);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 1));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.7f, 0.72f, 0.78f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.15f, 0.16f, 0.20f, 1.0f));

            bool edited = ImGui::InputTextWithHint(isFrom ? "##from" : "##to",
                hints[i], strs[i],
                ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            // --- активация режима автодополнения ---
            if (ImGui::IsItemActive()) {
                autoState.active = true;
                autoState.editingFrom = isFrom;
                mapViewer.setEditingFrom(isFrom);
                mapViewer.updateSuggestions();
            }
            if (edited) {
                mapViewer.buildPathFromAliases(fromStr, toStr);
            }

            ImVec2 a = ImGui::GetItemRectMin();
            ImVec2 b = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (ImGui::IsItemActive())
                dl->AddRect(a, b, ImGui::GetColorU32(ImVec4(0.0f, 0.45f, 0.95f, 1.0f)), 6.0f, 0, 2.0f);

            // === подсказки ===
            if (autoState.active && autoState.editingFrom == isFrom) {
                const auto& sugg = mapViewer.getCurrentSuggestions();
                if (!sugg.empty()) {
                    float itemH = 26.0f;
                    float totalH = std::min(itemH * (float)sugg.size(), 130.0f);
                    ImVec2 pos = ImVec2(a.x, b.y + 2.0f);
                    ImVec2 size = ImVec2(b.x - a.x, totalH);
                    ImGui::SetCursorScreenPos(pos);

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 0.96f));
                    ImGui::BeginChild(("AutoList" + std::to_string(i)).c_str(), size, true, ImGuiWindowFlags_NoScrollbar);

                    // обработка стрелок ↑↓ и Enter
                    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                        autoState.hovered = (autoState.hovered <= 0) ? (int)sugg.size() - 1 : autoState.hovered - 1;
                    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                        autoState.hovered = (autoState.hovered + 1) % (int)sugg.size();

                    for (int j = 0; j < sugg.size(); ++j) {
                        bool h = (j == autoState.hovered);
                        ImVec4 color = h ? ImVec4(0.05f, 0.45f, 0.95f, 0.2f) : ImVec4(1, 1, 1, 0);
                        if (h)
                            dl->AddRectFilled(ImVec2(a.x + 2, pos.y + j * itemH),
                                ImVec2(a.x + size.x - 2, pos.y + (j + 1) * itemH),
                                ImGui::GetColorU32(color), 4.0f);

                        if (ImGui::Selectable(sugg[j].c_str(), h)) {
                            if (autoState.editingFrom) fromStr = sugg[j];
                            else toStr = sugg[j];
                            mapViewer.clearSuggestions();
                            autoState.active = false;
                            autoState.hovered = -1;
                            ImGui::SetKeyboardFocusHere(-1);
                            break;
                        }
                    }

                    // Enter выбирает подсвеченный элемент
                    if (autoState.hovered >= 0 && ImGui::IsKeyPressed(ImGuiKey_Enter))
                    {
                        if (autoState.editingFrom) fromStr = sugg[autoState.hovered];
                        else toStr = sugg[autoState.hovered];
                        mapViewer.clearSuggestions();
                        autoState.active = false;
                        autoState.hovered = -1;
                        ImGui::SetKeyboardFocusHere(-1);
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                }
            }
            ImGui::Spacing();
        }

        ImGui::PopItemWidth();

        //------------------------------------------------------------------
        // 🚀 Кнопка — ровная и спокойная, без быстрых пульсаций
        //------------------------------------------------------------------
        float tNow = SDL_GetTicks() * 0.00025f;   // очень медленное дыхание цвета
        float pulse = 0.6f + 0.4f * sinf(tNow);   // едва заметное колебание
        ImVec4 baseColor = ImVec4(0.02f, 0.44f + 0.15f * pulse, 0.95f, 1.0f);
        ImVec4 hoverColor = ImVec4(0.10f, 0.55f + 0.10f * pulse, 1.00f, 1.0f);
        ImVec4 activeColor = ImVec4(0.00f, 0.50f, 1.00f, 1.0f);

        constexpr float BUTTON_HEIGHT = 45.0f;   // чуть выше стандартной строки, но не громоздко
        constexpr float BUTTON_WIDTH_PCT = 0.85f; // ширина кнопки = 85 % от панели

        ImVec2 avail = ImGui::GetContentRegionAvail();
        float btnWidth = avail.x * BUTTON_WIDTH_PCT;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - btnWidth) * 0.5f); // выравнивание по центру
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

        if (ImGui::Button("\uf4d7 Построить путь", ImVec2(btnWidth, BUTTON_HEIGHT)))
            mapViewer.buildPathFromAliases(fromStr, toStr);

        ImVec2 bPos = ImGui::GetItemRectMin();
        ImVec2 bSize = ImGui::GetItemRectSize();
        ImU32 shineTop = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        ImU32 shineBot = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
        dl->AddRectFilledMultiColor(
            ImVec2(bPos.x, bPos.y),
            ImVec2(bPos.x + bSize.x, bPos.y + bSize.y * 0.4f),
            shineTop, shineTop, shineBot, shineBot);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
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


// === Node Inspector ===
// === Верхняя панель навигации (чистый визуальный слой) ===
void UIManager::drawTopNavBar(MapViewer& viewer) {
    using namespace Layout;

    constexpr float HEADER_H = HEADER_HEIGHT;
    constexpr float LOGO_MARGIN_X = 24.0f;
    constexpr float LOGO_MARGIN_Y = 13.0f;
    constexpr float SEARCH_WIDTH = 420.0f;
    constexpr float SEARCH_HEIGHT = 32.0f;
    constexpr float BTN_WIDTH = 120.0f;
    constexpr float BTN_HEIGHT = 34.0f;
    constexpr float ITEM_SPACING = 18.0f;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screenW, HEADER_H), ImGuiCond_Always);

    // ==== Начало окна-шапки ====
    if (ImGui::Begin("\u1f9ed TopNavigation", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar))
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        // --- Подложка: лёгкий градиент фирменных оттенков MISIS ---
        ImU32 top = ImGui::GetColorU32(ImVec4(0.00f, 0.32f, 0.68f, 1.0f));   // глубокий синий #004C97
        ImU32 bottom = ImGui::GetColorU32(ImVec4(0.00f, 0.53f, 0.85f, 1.0f)); // светлый акцент #009EE3
        draw->AddRectFilledMultiColor(
            pos,
            ImVec2(pos.x + size.x, pos.y + size.y),
            top, top, bottom, bottom
        );

        // === Логотип MISIS ===
        ImGui::SetCursorPos(ImVec2(LOGO_MARGIN_X, -10.0f));
        if (logoTexture)
        {
            int texW = 0, texH = 0;
            SDL_QueryTexture(logoTexture, nullptr, nullptr, &texW, &texH);

            // Подгоним логотип в разумные пределы, чтобы точно был видим
            float maxLogoH = 80.0f;        // высота примерно под шапку
            float scale = maxLogoH / texH; // масштаб по высоте
            ImVec2 logoSize(texW * scale, texH * scale);

            ImGui::Image((ImTextureID)logoTexture, logoSize);
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 1, 1, 1), "MISIS Campus Map 2025");
        }

        // === Глобальный поиск ===
        float searchX = SIDEBAR_WIDTH + ITEM_SPACING * 2;
        float searchY = 14.0f;
        ImGui::SetCursorPos(ImVec2(searchX, searchY));

        static std::string globalSearchQuery;
        ImGui::PushItemWidth(SEARCH_WIDTH);

        // Форма поискового поля — белое, округлое
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1, 1, 1, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.12f, 0.18f, 1));

        ImGui::InputTextWithHint("##GlobalSearch",
            "\uf03a Найдите аудиторию, лабораторию или корпус...",
            &globalSearchQuery);

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        //--------------------------------------------------
        // 🔘 Справа — блок контрастных белых кнопок
        //--------------------------------------------------
        float btnY = (HEADER_H - BTN_HEIGHT) * 0.5f;

        // Правая кнопка (Полный экран)
        float btnRightMargin = 20.0f;         // отступ от правого края окна
        float fullBtnWidth = 150.0f;        // ширина кнопки "В полный экран"
        float langBtnWidth = 110.0f;        // ширина кнопки "Рус / Eng"
        float btnGap = 10.0f;         // зазор между кнопками

        float fullBtnX = screenW - fullBtnWidth - btnRightMargin;
        float langBtnX = fullBtnX - langBtnWidth - btnGap;

        // общие стили белых кнопок
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));   // чисто белая заливка
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.97f, 1.0f, 1.0f)); // лёгкий подсвет при hover
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.93f, 0.98f, 1.0f)); // мягкое нажатие
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.65f, 0.70f, 0.85f, 1.0f)); // серо‑голубая рамка

        // --- Кнопка языка ---
        ImGui::SetCursorPos(ImVec2(langBtnX, btnY));
        ImGui::Button("\u80ac  Рус / Eng", ImVec2(langBtnWidth, BTN_HEIGHT));
        ImGui::PopStyleColor(4);
        ImGui::SameLine();

        // --- кнопка "в полный экран" ---
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.97f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.93f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.65f, 0.70f, 0.85f, 1.0f));

        ImGui::SetCursorPos(ImVec2(fullBtnX, btnY));
        if (ImGui::Button("\u26f6  В полный экран", ImVec2(fullBtnWidth, BTN_HEIGHT))) {
            SDL_Window* win = SDL_GL_GetCurrentWindow();
            Uint32 flags = SDL_GetWindowFlags(win);
            bool isFull = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
            SDL_SetWindowFullscreen(win, isFull ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

// === Левая боковая панель (Sidebar / Избранное) ===
void UIManager::drawLeftSidebar(MapViewer& viewer) {
    using namespace Layout;

    static bool collapsed = false;
    static int  selected = -1;

    constexpr float ITEM_HEIGHT = 42.0f;
    constexpr float INDICATOR_W = 4.0f;
    constexpr float ICON_SIZE = 20.0f;
    constexpr float CARD_ALPHA = 0.98f;
    constexpr float PADDING_X = 18.0f;
    constexpr float PADDING_Y = 12.0f;

    ImGuiIO& io = ImGui::GetIO();
    float sidebarWidth = collapsed ? 72.0f : SIDEBAR_WIDTH;
    float sidebarHeight = io.DisplaySize.y - HEADER_HEIGHT;

    ImGui::SetNextWindowPos(ImVec2(0, HEADER_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight));
    ImGui::SetNextWindowBgAlpha(CARD_ALPHA);

    if (ImGui::Begin("📚 Sidebar", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar))
    {
        // --- Карточная подложка и рамка ---
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.97f)), 10.0f);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            ImGui::GetColorU32(ImVec4(0, 0, 0, 0.06f)), 10.0f, 0, 2.0f);

        // --- Гамбургер / заголовок меню ---
        ImGui::SetCursorPos(ImVec2(PADDING_X, PADDING_Y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.92f, 0.94f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.90f, 0.98f, 1.0f));
        if (ImGui::Button(collapsed ? "\u2630" : "\u2630  Меню", ImVec2(-1, 40)))
            collapsed = !collapsed;
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();

        // --- Основные пункты навигации ---
        static const std::pair<const char*, const char*> items[] = {
            { "\uf015", "Университет"},
            { "\uf518", "Библиотека"},
            { "\uf0f4", "Кафе и столовые" },
            { "\uf5a2", "Спортцентр" },
            { "\uf568", "Главная площадь" },
            { "\uf015", "Администрация" }
        };

        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            ImVec2 btnPos = ImGui::GetCursorScreenPos();
            ImVec2 btnSize = ImVec2(sidebarWidth - PADDING_X * 2, ITEM_HEIGHT);

            bool hovered = ImGui::IsMouseHoveringRect(btnPos,
                ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y));
            bool clicked = ImGui::InvisibleButton(
                ("##item" + std::to_string(i)).c_str(), btnSize);

            if (clicked) {
                selected = i;
                viewer.getInputFrom() = items[i].second;
                viewer.updateSuggestions();
            }

            // фон при наведении / выборе
            ImU32 bgColor = 0;
            if (selected == i)
                bgColor = ImGui::GetColorU32(ImVec4(0.05f, 0.44f, 0.88f, 1.0f));
            else if (hovered)
                bgColor = ImGui::GetColorU32(ImVec4(0.86f, 0.90f, 0.97f, 1.0f));

            if (bgColor)
                dl->AddRectFilled(btnPos,
                    ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                    bgColor, 8.0f);

            // Индикация выбранного
            if (selected == i)
                dl->AddRectFilled(ImVec2(btnPos.x + 3, btnPos.y + 3),
                    ImVec2(btnPos.x + 3 + INDICATOR_W, btnPos.y + btnSize.y - 3),
                    ImGui::GetColorU32(ImVec4(1.0f, 0.7f, 0.2f, 1.0f)),
                    3.0f);

            // Текст и иконка
            ImVec4 textCol = (selected == i)
                ? ImVec4(1, 1, 1, 1)
                : ImVec4(0.10f, 0.14f, 0.22f, 1.0f);
            float textY = btnPos.y + (ITEM_HEIGHT - ICON_SIZE) * 0.5f;

            dl->AddText(ImVec2(btnPos.x + 16.0f, textY),
                ImGui::GetColorU32(textCol), items[i].first);

            if (!collapsed) {
                dl->AddText(ImVec2(btnPos.x + 16.0f + 28.0f, textY),
                    ImGui::GetColorU32(textCol), items[i].second);
            }
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
    float screenH = io.DisplaySize.y;

    //---------------------------------------------
    // ГЕОМЕТРИЯ РАЗМЕЩЕНИЯ
    //---------------------------------------------
    const float PANEL_W = 250.0f;     // ширина блока
    const float BUTTON_H = 36.0f;     // стандартная высота кнопок
    const float GAP = 8.0f;           // промежутки
    const float MARGIN_RIGHT = 25.0f; // отступ от правой стороны
    const float PANEL_TOP = Layout::HEADER_HEIGHT + 250.0f; // под панелью маршрута

    // число этажей влияет на высоту панели
    int floorCount = 0;
    {
        const BuildingMeta* meta = mapViewer.getGraphManager().getBuildingMeta(mapViewer.getCurrentBuilding());
        if (meta)
            floorCount = static_cast<int>(meta->floors.size());
    }
    float PANEL_H = 300.0f + floorCount * (BUTTON_H + GAP);

    // конечные координаты угла окна:
    float panelX = screenW - PANEL_W - MARGIN_RIGHT;
    float panelY = PANEL_TOP;

    //---------------------------------------------
    // НАСТРОЙКА ОКНА
    //---------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(PANEL_W, PANEL_H));
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("🏢 BuildingFloorsPanel", nullptr, flags))
    {
        //---------------------------------------------
        // КНОПКА ГЛАВНОГО ВИДА (КАМПУС)
        //---------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("\uf279  Вид сверху (Кампус)", ImVec2(-1, BUTTON_H))) {
            mapViewer.switchViewToCampus();
        }

        ImGui::Dummy(ImVec2(0, GAP));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, GAP));

        //---------------------------------------------
        // ВЫПАДАЮЩИЙ СПИСОК КОРПУСОВ
        //---------------------------------------------
        const auto& metas = mapViewer.getGraphManager().buildingMetas;
        std::string currentBuilding = mapViewer.getCurrentBuilding();

        ImGui::TextColored(ImVec4(0.0f, 0.33f, 0.7f, 1.0f), "Корпус");
        ImGui::PushItemWidth(-1);

        const char* currentLabel =
            currentBuilding.empty() ? "Выберите корпус..." : currentBuilding.c_str();

        if (ImGui::BeginCombo("##buildingSelect", currentLabel)) {
            for (const auto& [bid, meta] : metas) {
                bool selected = (bid == currentBuilding);
                if (ImGui::Selectable(meta.name.c_str(), selected)) {
                    // при выборе сразу открываем 1 этаж корпуса
                    if (!meta.floors.empty())
                        mapViewer.switchToFloor(bid, meta.floors.front().floor);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0, GAP));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, GAP));

        //---------------------------------------------
        // КНОПКИ ЭТАЖЕЙ (СТОЛБИК СВЕРХУ ВНИЗ)
        //---------------------------------------------
        const BuildingMeta* bm = mapViewer.getGraphManager().getBuildingMeta(currentBuilding);
        if (bm && !bm->floors.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 0.33f, 0.7f, 1.0f), "Этажи");

            // формируем список этажей по убыванию
            std::vector<int> floors;
            for (auto& f : bm->floors)
                floors.push_back(f.floor);
            std::sort(floors.begin(), floors.end(), std::greater<int>());

            // кнопки по вертикали
            for (int f : floors) {
                bool active = (f == mapViewer.getCurrentFloor());
                ImVec4 col = active ?
                    ImVec4(0.08f, 0.50f, 1.0f, 1.0f) :
                    ImVec4(1.0f, 1.0f, 1.0f, 0.96f);
                ImGui::PushStyleColor(ImGuiCol_Button, col);

                std::string label = "Этаж " + std::to_string(f);
                if (ImGui::Button(label.c_str(), ImVec2(-1, BUTTON_H))) {
                    mapViewer.switchToFloor(currentBuilding, f);
                }
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0, GAP - 2.0f));
            }
        }
        else {
            ImGui::TextDisabled("Этажи отсутствуют");
        }

        ImGui::PopStyleVar();
    }
    ImGui::End();
}