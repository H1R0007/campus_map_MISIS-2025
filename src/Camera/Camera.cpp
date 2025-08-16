#include "Camera.hpp"
#include <algorithm>

Camera::Camera()
    : viewport{ 0, 0, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT },
    scale(1.0f)
{
    centerOnCanvas();
}

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

void Camera::clampViewport() {
    int scaledCanvasW = static_cast<int>(Config::CANVAS_WIDTH * scale);
    int scaledCanvasH = static_cast<int>(Config::CANVAS_HEIGHT * scale);

    if (scaledCanvasW <= viewport.w) {
        viewport.x = (scaledCanvasW - viewport.w) / 2;
    }
    else {
        viewport.x = std::clamp(viewport.x, 0, scaledCanvasW - viewport.w);
    }

    if (scaledCanvasH <= viewport.h) {
        viewport.y = (scaledCanvasH - viewport.h) / 2;
    }
    else {
        viewport.y = std::clamp(viewport.y, 0, scaledCanvasH - viewport.h);
    }
}

void Camera::update() {
    clampViewport();
}

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