#include "Camera.hpp"
#include <algorithm>

// === Constructor ===
Camera::Camera()
    : viewport{ 0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT },
    scale(1.0f),
    worldWidth(Config::CANVAS_WIDTH),   // по умолчанию canvas
    worldHeight(Config::CANVAS_HEIGHT)
{
    centerOnCanvas();
}

// === Viewport & World size ===
void Camera::setWorldSize(int width, int height) {
    worldWidth = width;
    worldHeight = height;
    clampViewport();
}

void Camera::setViewportSize(int w, int h) {
    viewport.w = w;
    viewport.h = h;
    clampViewport();
}

// === Scale / Zoom ===
void Camera::setScale(float newScale) {
    scale = std::clamp(newScale, Config::MIN_ZOOM, Config::MAX_ZOOM);
    update();
}

void Camera::zoom(float zoomFactor, ZoomMode mode, const SDL_Point& screenPos) {
    float oldScale = scale;
    float newScale = std::clamp(oldScale * zoomFactor, Config::MIN_ZOOM, Config::MAX_ZOOM);

    if (newScale == oldScale) return;

    if (mode == ZoomMode::Center) {
        SDL_Point screenCenter = { viewport.w / 2, viewport.h / 2 };
        SDL_Point worldBefore = screenToWorld(screenCenter);
        scale = newScale;
        SDL_Point worldAfter = screenToWorld(screenCenter);

        viewport.x += (worldBefore.x - worldAfter.x) * scale;
        viewport.y += (worldBefore.y - worldAfter.y) * scale;
    }
    else if (mode == ZoomMode::Mouse) {
        SDL_Point worldBefore = screenToWorld(screenPos);
        scale = newScale;
        SDL_Point worldAfter = screenToWorld(screenPos);

        viewport.x += (worldBefore.x - worldAfter.x) * scale;
        viewport.y += (worldBefore.y - worldAfter.y) * scale;
    }

    clampViewport();
}

// === Movement / Centering ===
void Camera::move(int dx, int dy) {
    viewport.x += dx;
    viewport.y += dy;
    clampViewport();
}

void Camera::centerOnCanvas() {
    int scaledCanvasW = static_cast<int>(Config::CANVAS_WIDTH * scale);
    int scaledCanvasH = static_cast<int>(Config::CANVAS_HEIGHT * scale);

    viewport.x = std::max(0, (scaledCanvasW - viewport.w) / 2);
    viewport.y = std::max(0, (scaledCanvasH - viewport.h) / 2);
    clampViewport();
}

// === Update ===
void Camera::update() {
    clampViewport();
}

// === Coordinate transforms ===
SDL_Point Camera::worldToScreen(const SDL_Point& world) const {
    SDL_Point scr;
    scr.x = static_cast<int>((world.x * scale) - viewport.x);
    scr.y = static_cast<int>((world.y * scale) - viewport.y);
    return scr;
}

SDL_Point Camera::screenToWorld(const SDL_Point& screen) const {
    SDL_Point world;
    world.x = static_cast<int>((viewport.x + screen.x) / scale);
    world.y = static_cast<int>((viewport.y + screen.y) / scale);
    return world;
}

// === Helpers ===
void Camera::clampViewport() {
    int scaledCanvasW = static_cast<int>(worldWidth * scale);
    int scaledCanvasH = static_cast<int>(worldHeight * scale);

    // "коридор" вокруг карты (% от окна)
    int padX = viewport.w / 5;
    int padY = viewport.h / 5;

    // Горизонталь
    if (scaledCanvasW <= viewport.w) {
        // Карта уже меньше окна → центрируем и даём двигать ±padX
        viewport.x = (scaledCanvasW - viewport.w) / 2;
        viewport.x = std::clamp(viewport.x, -padX, padX);
    }
    else {
        // Карта больше окна → коридор по обеим сторонам
        viewport.x = std::clamp(viewport.x, -padX,
            scaledCanvasW - viewport.w + padX);
    }

    // Вертикаль
    if (scaledCanvasH <= viewport.h) {
        viewport.y = (scaledCanvasH - viewport.h) / 2;
        viewport.y = std::clamp(viewport.y, -padY, padY);
    }
    else {
        viewport.y = std::clamp(viewport.y, -padY,
            scaledCanvasH - viewport.h + padY);
    }
}