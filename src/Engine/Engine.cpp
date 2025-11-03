#include "Engine.hpp"
#include "../config.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../UI/UIManager.hpp"
#include <SDL2/SDL_ttf.h>
#include <iostream>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

// --- Lifecycle ---
Engine::Engine(const char* title, int w, int h)
    : window(nullptr, SDL_DestroyWindow),       // Инициализируем умные указатели
    renderer(nullptr, SDL_DestroyRenderer),
    isRunning(true),
    currentWidth(Config::INITIAL_WINDOW_WIDTH),
    currentHeight(Config::INITIAL_WINDOW_HEIGHT)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        throw std::runtime_error("SDL Init failed");
    }

    if (TTF_Init() == -1) {
        std::cerr << "Failed to init TTF: " << TTF_GetError() << std::endl;
        SDL_Quit();
        throw std::runtime_error("TTF Init failed");
    }

    SDL_StartTextInput();

    // Используем .reset() для присваивания значения
    window.reset(SDL_CreateWindow(
        Config::WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        currentWidth,
        currentHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    ));
    if (!window) {
        throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
    }

    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
    if (!renderer) {
        throw std::runtime_error("SDL_CreateRenderer failed: " + std::string(SDL_GetError()));
    }

    initImGui();

    // `std::make_unique` - безопасный способ создания
    mapViewer = std::make_unique<MapViewer>(renderer.get());
    uiManager = std::make_unique<UIManager>();
}

Engine::~Engine() {
    shutdownImGui();

    SDL_StopTextInput();
    if (TTF_WasInit()) TTF_Quit();
    SDL_Quit();
}

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Включить навигацию с клавиатуры
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Включить Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Включить Viewports

    ImGui::StyleColorsDark(); // или StyleColorsLight(), StyleColorsClassic()

    // Настройка Viewports
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Инициализация бэкендов
    ImGui_ImplSDL2_InitForSDLRenderer(window.get(), renderer.get());
    ImGui_ImplSDLRenderer2_Init(renderer.get());
}

void Engine::shutdownImGui() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}


// --- Run loops ---
void Engine::run() {
    const Uint32 frameDelay = 16;
    while (isRunning) {
        Uint32 start = SDL_GetTicks();

        handleEvents();
        render();

        Uint32 frameTime = SDL_GetTicks() - start;
        if (frameDelay > frameTime)
            SDL_Delay(frameDelay - frameTime);
    }
}

void Engine::handleFrame() {
    // Called by emscripten main loop -> single frame only
    handleEvents();
    render();
}

// --- Event handling ---
void Engine::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT) stop();

        // Window fullscreen toggle
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
            Uint32 flags = SDL_GetWindowFlags(window.get());
            SDL_SetWindowFullscreen(window.get(),
                (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
        }

        // DEV mode toggle
#ifndef __EMSCRIPTEN__
// DEV mode toggle (F2) - only for native builds
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2) {
            if (Config::BUILD_DEV) {
                Config::toggleDevMode();
                std::cout << "Switched mode: "
                    << (Config::DEV_MODE ? "DEV" : "USER") << "\n";
            }
        }
#endif
        // Window resize -> propagate to camera
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            currentWidth = event.window.data1;
            currentHeight = event.window.data2;

            // Сообщаем MapViewer об изменении размера
            mapViewer->onWindowResized(currentWidth, currentHeight);
        }
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
            // Если ImGui хочет мышь или клавиатуру, не передаем событие в MapViewer
        }
        else {
            mapViewer->handleEvent(event);
        }
    }
}

// --- Rendering ---
void Engine::render() {
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    if (uiManager) {
        mapViewer->updateSuggestions();
        uiManager->render(*mapViewer);
    }

    SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
    SDL_RenderClear(renderer.get());                         

    mapViewer->render();      // map

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.get());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_RenderPresent(renderer.get());           
}
