#include "Engine.hpp"
#include "../config.hpp"
#include "../Map_Visuality/Map_Viewer.hpp"
#include "../UI/UIManager.hpp"
#include "../UI/UITheme.hpp"
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
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

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << std::endl;
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
    io.Fonts->AddFontFromFileTTF("assets/UI/fonts/Roboto-Regular.ttf", 18.0f);

    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.PixelSnapH = true;
    static const ImWchar emoji_range[] = { 0x2600, 0x27BF, 0 };
    io.Fonts->AddFontFromFileTTF("assets/UI/fonts/NotoEmoji-Regular.ttf", 18.0f, &cfg, emoji_range);

    cfg.MergeMode = true;
    static const ImWchar fa_range[] = { 0xf000, 0xf3ff, 0 };
    io.Fonts->AddFontFromFileTTF("assets/UI/fonts/fa-solid-900.ttf", 16.0f, &cfg, fa_range);

    if (io.Fonts->Fonts.empty())
        std::cerr << "[ImGui] Font atlas failed to load!" << std::endl;
    else
        std::cout << "[ImGui] Font atlas built, total "
        << io.Fonts->Fonts.size() << " fonts merged.\n";

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    SetupImGuiThemeMISIS();
    // ——————————————————

    ImGui_ImplSDL2_InitForSDLRenderer(window.get(), renderer.get());
    ImGui_ImplSDLRenderer2_Init(renderer.get());

    uiManager = std::make_unique<UIManager>();
    mapViewer = std::make_unique<MapViewer>(renderer.get());

    // === Загружаем логотип MISIS ===
    SDL_Surface* surface = IMG_Load("assets/UI/icons/MISIS_logo.png");
    std::cout << "\n[UI] === Logo load sequence ===\n";
    std::cout << "[UI] IMG_Load path = assets/UI/icons/MISIS_logo.png\n";

    if (!surface) {
        std::cerr << "[UI] Failed to load PNG: " << IMG_GetError() << std::endl;
    }
    else {
        std::cout << "[UI] Surface loaded successfully.\n"
            << "      w=" << surface->w << " h=" << surface->h
            << " pitch=" << surface->pitch << std::endl;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer.get(), surface);
        if (!tex) {
            std::cerr << "[UI] SDL_CreateTextureFromSurface failed: "
                << SDL_GetError() << std::endl;
        }
        else {
            std::cout << "[UI] Texture created OK: " << tex << std::endl;
        }

        SDL_FreeSurface(surface);

        if (uiManager) {
            uiManager->setLogoTexture(tex);
            std::cout << "[UI] Texture pointer passed to UIManager.\n";
        }
        else {
            std::cerr << "[UI] uiManager pointer is NULL!\n";
        }
    }
    std::cout << "[UI] ==========================\n";
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

void Engine::render() {
    // --- Начало нового кадра ImGui ---
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (isLoading) {
        drawLoadingScreen();

        // Завершение кадра (чтобы ImGui сбалансировался)
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.get());
        SDL_RenderPresent(renderer.get());
        return;
    }

    SDL_SetRenderDrawColor(renderer.get(), 235, 238, 243, 255);
    SDL_RenderClear(renderer.get());
    mapViewer->render();

    // --- Обычный UI ---
    if (uiManager) {
        mapViewer->updateSuggestions();
        uiManager->render(*mapViewer);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.get());
    SDL_RenderPresent(renderer.get());
}

void Engine::drawLoadingScreen() {
    ImGuiIO& io = ImGui::GetIO();
    float w = io.DisplaySize.x;
    float h = io.DisplaySize.y;
    if (w < 1.0f || h < 1.0f) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 center(w * 0.5f, h * 0.5f);

    loadingTimer += io.DeltaTime;

    const float phaseGrowEnd = 1.0f;  // растёт
    const float phaseHoldEnd = 1.3f;  // пауза
    const float phaseShrinkEnd = 2.2f;  // сжимается
    const float totalTime = phaseShrinkEnd;

    float Rmax = std::sqrtf(w * w + h * h) * 0.5f;
    float radius;
    ImVec4 circleColor = ImVec4(0.12f, 0.35f, 0.85f, 1.0f); // насыщенный синий
    const char* txt = "Loading Campus Data…";

    float textAlpha = 1.f;
    float revealAlpha = 0.f;  // альфа для карты (0 = невидима, 1 = полностью видна)

    // === Определяем текущие параметры фаз ===
    if (loadingTimer <= phaseGrowEnd) {
        // ФАЗА 1: РАСШИРЕНИЕ — ЧЁРНЫЙ ФОН, СИНИЙ КРУГ РАСТЁТ
        float t = loadingTimer / phaseGrowEnd;
        radius = Rmax * (1.f - std::cos(t * M_PI * 0.5f)); // ease-out
        textAlpha = 1.f;
        revealAlpha = 0.f;
    }
    else if (loadingTimer <= phaseHoldEnd) {
        // ФАЗА 2: ПАУЗА — КРУГ ДОСТИГ ПОЛНОГО РАЗМЕРА
        radius = Rmax;
        float t = (loadingTimer - phaseGrowEnd) / (phaseHoldEnd - phaseGrowEnd);
        textAlpha = 1.f - t; // медленно гасим текст
        revealAlpha = 0.f;
    }
    else {
        // ФАЗА 3: СУЖЕНИЕ — ПОД СИНИМ СЛОЕМ ПРОЯВЛЯЕТСЯ КАРТА
        float t = (loadingTimer - phaseHoldEnd) / (phaseShrinkEnd - phaseHoldEnd);
        t = std::clamp(t, 0.f, 1.f);
        radius = Rmax * (1.f - t); // сужается обратно
        revealAlpha = t;           // карта проявляется при сужении
        textAlpha = 0.f;
    }

    // === Сначала чёрный фон ===
    SDL_SetRenderDrawColor(renderer.get(), 235, 238, 243, 255);
    SDL_RenderClear(renderer.get());

    // === Проявляем карту в фоне (при сжатии круга) ===
    if (mapViewer && revealAlpha > 0.0f) {
        // рендерим карту "под" синим кругом, но делаем альфа-переход
        // (реализуется через смешение на более низком слое)
        ImVec4 fadeColor(1.f, 1.f, 1.f, revealAlpha); // плавная прозрачность
        // простой вызов отрисовки, карта всегда рисуется полностью
        mapViewer->render();
        // сам ImGui не управляет альфа SDL, так что можно визуальный fade воспринять через радиус
    }

    // === Синий круг ===
    draw->AddCircleFilled(center, radius, ImGui::GetColorU32(circleColor), 128);

    // === Текст ===
    if (textAlpha > 0.f) {
        ImVec2 ts = ImGui::CalcTextSize(txt);
        draw->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f),
            ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, textAlpha)), txt);
    }

    // === Эффект проявления карты при сжатии круга ===
    // Когда радиус уменьшается, видимая часть карты увеличивается
    if (loadingTimer > phaseHoldEnd) {
        const int segs = 80;
        const float twoPi = 2.0f * M_PI;
        const float step = twoPi / segs;
        // Цвет слоя поверх карты (тот же синий, но становится прозрачным)
        ImU32 coverCol = ImGui::GetColorU32(ImVec4(circleColor.x, circleColor.y, circleColor.z, 1.0f - revealAlpha));

        for (int i = 0; i < segs; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;

            ImVec2 p0(center.x + cosf(a0) * Rmax, center.y + sinf(a0) * Rmax);
            ImVec2 p1(center.x + cosf(a1) * Rmax, center.y + sinf(a1) * Rmax);
            ImVec2 q0(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
            ImVec2 q1(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);

            // Слой вокруг круга, который съёживается, открывая карту
            draw->AddQuadFilled(p0, p1, q1, q0, coverCol);
        }
    }

    // === Завершение ===
    if (loadingTimer >= totalTime) {
        isLoading = false;
    }
}