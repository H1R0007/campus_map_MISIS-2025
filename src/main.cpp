#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "Engine/Engine.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
static Engine* appPtr = nullptr;

// === WebAssembly Main Loop Wrapper ===
// This function is called by Emscripten every frame.
static void main_loop() {
    if (appPtr) {
        appPtr->handleFrame();
    }
}
#endif

// === Entry Point ===
int main(int argc, char* argv[]) {
    static Engine app("Campus Map", 800, 600);

#ifdef __EMSCRIPTEN__
    appPtr = &app;
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    app.run(); // Native/Desktop version
#endif

    return 0;
}