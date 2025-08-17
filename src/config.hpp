#pragma once

namespace Config {
    // Окно
    constexpr int WINDOW_WIDTH = 800;
    constexpr int WINDOW_HEIGHT = 600;
    constexpr const char* WINDOW_TITLE = "Campus Map";

    // Карта
    constexpr const char* MAP_PATH = "assets/Map.png";
    constexpr int MAP_WIDTH = 1476;
    constexpr int MAP_HEIGHT = 780;

    // Рабочее поле (Canvas)
    constexpr int CANVAS_WIDTH = 2000;
    constexpr int CANVAS_HEIGHT = 2000;

    // Масштаб
    constexpr float MIN_ZOOM = 0.5f;
    constexpr float MAX_ZOOM = 3.0f;

    // Developer mode flag 
    constexpr bool DEV_MODE = true; // переключать перед релизом!

    //путь к json
    constexpr const char* NODES_PATH = "assets/nodes.json";
}