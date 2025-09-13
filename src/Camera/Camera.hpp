#pragma once
#include <SDL2/SDL.h>
#include "../config.hpp"

// === Zoom behavior modes ===
enum class ZoomMode {
    Center,   // zoom relative to screen center
    Mouse     // zoom relative to mouse position
};

// === Camera ===
// Responsible for mapping between world coordinates (canvas)
// and screen coordinates (viewport). Handles zoom and pan.
class Camera {
public:
    Camera();

    // === Viewport & World size ===
    void setWorldSize(int width, int height);
    void setViewportSize(int w, int h);

    // === Scale / Zoom ===
    void setScale(float newScale);
    void zoom(float zoomFactor, ZoomMode mode, const SDL_Point& screenPos = { 0,0 });

    // === Movement / Centering ===
    void move(int dx, int dy);
    void centerOnCanvas();

    // === Update ===
    void update();

    // === Coordinate transforms ===
    SDL_Point worldToScreen(const SDL_Point& world) const;
    SDL_Point screenToWorld(const SDL_Point& screen) const;

    // === Accessors ===
    const SDL_Rect& getViewport() const { return viewport; }
    float getScale() const { return scale; }

private:
    // === Internal state ===
    SDL_Rect viewport;
    float scale;
    int worldWidth;
    int worldHeight;

    // === Helpers ===
    void clampViewport();
};