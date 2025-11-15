#include "Camera.hpp"
#include <cmath>

// === Constructor ===
Camera::Camera()
    : viewport{ 0, 0, Config::INITIAL_WINDOW_WIDTH, Config::INITIAL_WINDOW_HEIGHT },
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

    if (std::abs(scale - 1.0f) < 0.0001f) {
        centerOnCanvas();
    }
}

// === Scale / Zoom ===
void Camera::setScale(float newScale) {
    scale = std::clamp(newScale, Config::MIN_ZOOM, Config::MAX_ZOOM);
    clampViewport();

}

void Camera::zoom(float zoomFactor, ZoomMode mode, const SDL_Point& screenPos) {

    if (zoomFactor > 1.5f) zoomFactor = 1.5f;
    if (zoomFactor < 0.7f) zoomFactor = 0.7f;

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

// === Coordinate transforms ===
SDL_Point Camera::worldToScreen(const SDL_Point& world) const {
    SDL_Point scr;
    scr.x = static_cast<int>(std::lround(world.x * scale - viewport.x));
    scr.y = static_cast<int>(std::lround(world.y * scale - viewport.y));
    return scr;
}

SDL_Point Camera::screenToWorld(const SDL_Point& screen) const {
    SDL_Point world;
    world.x = static_cast<int>(std::lround((viewport.x + screen.x) / scale));
    world.y = static_cast<int>(std::lround((viewport.y + screen.y) / scale));
    return world;
}

// === Helpers ===
void Camera::clampViewport() {
    int scaledW = static_cast<int>(worldWidth * scale);
    int scaledH = static_cast<int>(worldHeight * scale);
    int padX = viewport.w / 5;
    int padY = viewport.h / 5;

    auto clampVal = [](int val, int minV, int maxV) {
        return std::max(minV, std::min(val, maxV));
        };

    if (scaledW <= viewport.w) {
        viewport.x = clampVal((scaledW - viewport.w) / 2, -padX, padX);
    }
    else {
        viewport.x = clampVal(viewport.x, -padX, scaledW - viewport.w + padX);
    }

    if (scaledH <= viewport.h) {
        viewport.y = clampVal((scaledH - viewport.h) / 2, -padY, padY);
    }
    else {
        viewport.y = clampVal(viewport.y, -padY, scaledH - viewport.h + padY);
    }
}

SDL_Point Camera::getCenterWorld() const {
    int cx = static_cast<int>(viewport.x / scale + (viewport.w * 0.5f) / scale);
    int cy = static_cast<int>(viewport.y / scale + (viewport.h * 0.5f) / scale);
    return { cx, cy };
}

void Camera::setCenterWorld(const SDL_Point& c) {
    viewport.x = static_cast<int>(c.x * scale - (viewport.w * 0.5f));
    viewport.y = static_cast<int>(c.y * scale - (viewport.h * 0.5f));
    clampViewport();
}