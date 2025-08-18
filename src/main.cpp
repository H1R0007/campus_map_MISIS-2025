#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "Engine/Engine.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
static Engine* appPtr = nullptr;

// Функция-обёртка: один кадр
static void main_loop() {
    if (appPtr) {
        appPtr->handleFrame();
    }
}
#endif

int main(int argc, char* argv[]) {
    static Engine app("Campus Map", 800, 600);

#ifdef __EMSCRIPTEN__
    appPtr = &app;
    // Emscripten сам крутит цикл, вызывая main_loop() на каждый кадр
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    app.run(); // Desktop/Native вариант
#endif
    return 0;
}