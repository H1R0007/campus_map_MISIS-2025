#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include <cstdio>

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
bool          gRunning = true;

int playerX = 100, playerY = 100;
int playerSize = 24;

void draw_grid(int cell = 64) {
    SDL_SetRenderDrawColor(gRenderer, 60, 60, 70, 255);
    for (int x = 0; x < 2000; x += cell) {
        SDL_RenderDrawLine(gRenderer, x, 0, x, 2000);
    }
    for (int y = 0; y < 2000; y += cell) {
        SDL_RenderDrawLine(gRenderer, 0, y, 2000, y);
    }
}

void frame() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
            gRunning = false;
        }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
#ifdef __EMSCRIPTEN__
                emscripten_cancel_main_loop();
#endif
                gRunning = false;
            }
            const int step = 10;
            if (e.key.keysym.sym == SDLK_LEFT)  playerX -= step;
            if (e.key.keysym.sym == SDLK_RIGHT) playerX += step;
            if (e.key.keysym.sym == SDLK_UP)    playerY -= step;
            if (e.key.keysym.sym == SDLK_DOWN)  playerY += step;
        }
    }

    SDL_SetRenderDrawColor(gRenderer, 30, 30, 35, 255);
    SDL_RenderClear(gRenderer);

    draw_grid();

    SDL_Rect r{ playerX, playerY, playerSize, playerSize };
    SDL_SetRenderDrawColor(gRenderer, 220, 80, 80, 255);
    SDL_RenderFillRect(gRenderer, &r);

    SDL_RenderPresent(gRenderer);
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    gWindow = SDL_CreateWindow("Campus Map",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN);
    if (!gWindow) {
        std::fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) {
        std::fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, true);
#else
    while (gRunning) frame();
#endif

    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}