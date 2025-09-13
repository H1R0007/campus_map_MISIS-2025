#pragma once

namespace Config {
    // Окно
    inline int WINDOW_WIDTH = 800;
    inline int WINDOW_HEIGHT = 600;
    constexpr const char* WINDOW_TITLE = "Campus Map";

    // Карта
    // --- КАРТЫ ---
    // Кампус
    constexpr const char* CAMPUS_GRAPH_PATH = "assets/campus/graph.json";
    constexpr const char* CAMPUS_META_PATH = "assets/campus/meta.json";
    constexpr const char* CAMPUS_MAP_PATH = "assets/campus/map.png";

    // Переходы (порталы)
    constexpr const char* TRANSITIONS_PATH = "assets/transitions/transitions.json";


    constexpr int MAP_WIDTH = 1476;
    constexpr int MAP_HEIGHT = 780;

    // Рабочее поле (Canvas)
    constexpr int CANVAS_WIDTH = 2000;
    constexpr int CANVAS_HEIGHT = 2000;

    // Масштаб
    constexpr float MIN_ZOOM = 0.5f;
    constexpr float MAX_ZOOM = 3.0f;

    constexpr const char* NODES_PATH = "assets/nodes.json";
    constexpr const char* ALIASES_PATH = "assets/aliases.json";

    // Путь к шрифту (чтобы было явно)
    constexpr const char* FONT_PATH = "assets/fonts/Roboto-Regular.ttf";

    // compile‑time: какой билд
    constexpr bool BUILD_DEV = true;   // при релизе ставите false

    // runtime: текущее состояние dev режима
    inline bool DEV_MODE = BUILD_DEV;

    inline void toggleDevMode() {
        if constexpr (BUILD_DEV) {
            DEV_MODE = !DEV_MODE;
        }
    }

    inline void forceUserMode() {
        DEV_MODE = false;
    }

    constexpr int LINE_POINT_SPACING = 50;   // шаг по умолчанию
    constexpr int LINE_POINT_MIN_SPACING = 5;    // минимально возможный шаг
    constexpr int LINE_POINT_MAX_COUNT = 100;    // максимум точек за раз

}