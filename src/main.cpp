#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "Engine/Engine.hpp"

int main(int argc, char* argv[]) {
    Engine app("Campus Map", 800, 600);
    app.run();
    return 0;
}