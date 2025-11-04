#pragma once
#include "imgui.h"

// ================================================================
//  MISIS Web‑style Theme — мягкие тени, карточные панели, акценты
// ================================================================
inline void SetupImGuiThemeMISIS() {
    ImGuiStyle& st = ImGui::GetStyle();

    // Геометрия
    st.WindowRounding = 10.0f;
    st.FrameRounding = 8.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.FramePadding = ImVec2(10, 6);
    st.ItemSpacing = ImVec2(12, 10);
    st.ScrollbarSize = 14.0f;
    st.WindowBorderSize = 0.0f;
    st.ChildBorderSize = 0.0f;
    st.FrameBorderSize = 0.0f;
    st.WindowPadding = ImVec2(16, 16);
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    // Цветовая палитра
    auto c = [](float r, float g, float b, float a = 1.0f)
        { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a); };

    ImVec4 mainBg = c(22, 25, 31, 240); // общий фон
    ImVec4 cardBg = c(28, 31, 39, 255); // фон окон/«карточек»
    ImVec4 accent = c(35, 119, 245);      // фирменный голубой
    ImVec4 accentHL = c(55, 139, 255);
    ImVec4 textMain = c(240, 240, 245);
    ImVec4 textMuted = c(180, 185, 195);
    ImVec4 divider = c(60, 65, 75);
    ImVec4 successCol = c(70, 190, 110);
    ImVec4 warnCol = c(255, 160, 35);

    ImVec4* col = st.Colors;
    col[ImGuiCol_Text] = textMain;
    col[ImGuiCol_TextDisabled] = textMuted;
    col[ImGuiCol_WindowBg] = cardBg;
    col[ImGuiCol_ChildBg] = cardBg;
    col[ImGuiCol_PopupBg] = c(25, 27, 34, 250);
    col[ImGuiCol_Border] = divider;
    col[ImGuiCol_FrameBg] = c(37, 40, 48);
    col[ImGuiCol_FrameBgHovered] = accent;
    col[ImGuiCol_FrameBgActive] = accentHL;
    col[ImGuiCol_TitleBgActive] = accent;
    col[ImGuiCol_CheckMark] = accentHL;
    col[ImGuiCol_SliderGrab] = accent;
    col[ImGuiCol_SliderGrabActive] = accentHL;
    col[ImGuiCol_Button] = accent;
    col[ImGuiCol_ButtonHovered] = accentHL;
    col[ImGuiCol_ButtonActive] = accentHL;
    col[ImGuiCol_Header] = accent;
    col[ImGuiCol_HeaderHovered] = accentHL;
    col[ImGuiCol_Separator] = divider;
    col[ImGuiCol_SeparatorHovered] = accent;
    col[ImGuiCol_SeparatorActive] = accentHL;
    col[ImGuiCol_ResizeGrip] = accent;
    col[ImGuiCol_ResizeGripHovered] = accentHL;

    // Любое легкое тёмное оформление при Viewports
    ImGui::StyleColorsDark(&st);
}