#pragma once
#include <SDL2/SDL.h>
#include "../config.hpp"

enum class ZoomMode {
    Center,    // центр экрана
    Mouse      // под курсором
};

class Camera {
public:
    Camera();

    void setScale(float newScale);
    void zoom(float zoomFactor, ZoomMode mode, const SDL_Point& screenPos = { 0,0 });
    void move(int dx, int dy);
    void centerOnCanvas();

    void update();
    SDL_Point worldToScreen(const SDL_Point& world) const;
    SDL_Point screenToWorld(const SDL_Point& screen) const;

    const SDL_Rect& getViewport() const { return viewport; }
    float getScale() const { return scale; }

private:
    SDL_Rect viewport;
    float scale;

    void clampViewport();
};