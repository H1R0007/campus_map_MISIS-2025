#pragma once
#include <SDL2/SDL.h>

// Forward-declare classes to avoid circular dependencies
class MapViewer;
class Camera;
class GraphManager;

/**
 * @class InputHandler
 * @brief Manages all user input (keyboard, mouse) and translates it into actions.
 *
 * This class is the single point of entry for SDL_Events. It separates UI-specific
 * input (like typing in a text field) from general application input (like panning the map).
 * It holds references to the main components it needs to modify or query (MapViewer, Camera, etc.).
 */
class InputHandler {
public:
    InputHandler(MapViewer& viewer, Camera& camera, GraphManager& graphManager);

    /**
     * @brief The main entry point for all SDL events.
     * @param event The SDL_Event to process.
     */
    void handleEvent(const SDL_Event& event);

private:
    // References to core application components
    MapViewer& mapViewer;
    Camera& camera;
    GraphManager& graphManager;

    // Private methods for organized event handling
    void handleKeyDown(const SDL_Event& event);
    void handleMouseButtonDown(const SDL_Event& event);
    void handleMouseMotion(const SDL_Event& event);
    void handleMouseWheel(const SDL_Event& event);
    void handleTextInput(const SDL_Event& event);

    // Private methods for specific input contexts
    void processDevMouseInput(const SDL_Event& event);
    void processDevKeyInput(const SDL_Event& event);
};