#pragma once
#include "imgui.h"

// ================================================================
//  MISIS Web‑style Theme — мягкие тени, карточные панели, акценты
// ================================================================
inline void SetupImGuiThemeMISIS() {
    ImGuiStyle& st = ImGui::GetStyle();

    st.WindowRounding = 12.0f;
    st.FrameRounding = 10.0f;
    st.GrabRounding = 8.0f;
    st.TabRounding = 8.0f;
    st.FramePadding = ImVec2(12, 8);
    st.ItemSpacing = ImVec2(12, 10);
    st.WindowPadding = ImVec2(20, 20);
    st.ScrollbarSize = 14.0f;
    st.WindowBorderSize = 0.0f;

    auto c = [](float r, float g, float b, float a = 1.0f)
        { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a); };

    // --- Фирменная визуальная база MISIS GlassPanel 2025 ---
    ImVec4 appBg = c(240, 243, 247);   // фон приложения (тёплый серовато-белый)
    ImVec4 cardBg = c(255, 255, 255, 235); // карточное "стекло"
    ImVec4 border = c(210, 214, 219);   // тонкая окантовка окон
    ImVec4 accent = c(0, 99, 204);      // фирменный синий
    ImVec4 accentHL = c(0, 120, 255);

    ImVec4 textMain = c(25, 30, 45);
    ImVec4 textMuted = c(120, 128, 145);

    ImVec4* col = st.Colors;
    col[ImGuiCol_WindowBg] = cardBg;
    col[ImGuiCol_ChildBg] = cardBg;
    col[ImGuiCol_PopupBg] = c(255, 255, 255, 248);
    col[ImGuiCol_Border] = border;
    col[ImGuiCol_Text] = textMain;
    col[ImGuiCol_TextDisabled] = textMuted;
    col[ImGuiCol_Button] = accent;
    col[ImGuiCol_ButtonHovered] = accentHL;
    col[ImGuiCol_ButtonActive] = accentHL;
    col[ImGuiCol_FrameBg] = c(242, 245, 250);
    col[ImGuiCol_FrameBgHovered] = accent;
    col[ImGuiCol_Header] = accent;

    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = appBg;
}