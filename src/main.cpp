#define SDL_MAIN_HANDLED
#include <iostream>
#include <SDL.h>
#include "Engine/Engine.hpp"
#include "bridge/SimpleBridge.hpp"  // Добавьте этот include

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

static Engine* appPtr = nullptr;

static void main_loop() {
    if (appPtr) {
        appPtr->handleFrame();
    }
}
#endifссв

int main(int argc, char* argv[]) {
#ifdef __EMSCRIPTEN__
    // Для WASM - минимальная инициализация
    std::cout << "Campus Map WASM Module Initialized" << std::endl;

    // ReactBridge уже зарегистрирован через EMSCRIPTEN_BINDINGS
    // Не создаем Engine для WASM чтобы избежать проблем с SDL

    // Emscripten main loop не нужен для React версии
    return 0;
#else
    // Нативная версия
    static Engine app("Campus Map", 800, 600);
    appPtr = &app;
    app.run();
    return 0;
#endif
}