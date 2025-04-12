/*
 * @author: Avidel
 * @LastEditors: Avidel
 */
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

static inline SDL_Renderer* renderer;
/**
 * @struct AppContext
 * @brief Contains the main SDL-related context for the application.
 * 
 * This structure holds all the necessary SDL objects and state information
 * required for the application to run, including window, renderer, textures,
 * audio device, and application state.
 * 
 * @var window          The SDL window handle
 * @var renderer        The SDL renderer associated with the window
 * @var message_tex     Texture for displaying text messages
 * @var image_tex       Texture for displaying images
 * @var message_dest    Rectangle defining the position and size of the message texture
 * @var audio_device    ID of the SDL audio device
 * @var app_quit        Flag indicating whether the application should continue running or quit
 */
struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture *message_tex, *image_tex;
    SDL_FRect message_dest;
    SDL_AudioDeviceID audio_device;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppInit(void**  /*appstate*/, int  /*argc*/, char*  /*argv*/[]) {
    SDL_SetAppMetadata("Example Renderer", "1.0", "Example-Renderer");
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("Hello, SDL3!");
    SDL_Window* window(
        SDL_CreateWindow("Hello, SDL3!", 800, 600, SDL_WINDOW_RESIZABLE));
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppContext*>(appstate);
    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult  /*result*/) {
    auto* app = static_cast<AppContext*>(appstate);
    if (app) {
        if (app->renderer) SDL_DestroyRenderer(app->renderer);
        if (app->window) SDL_DestroyWindow(app->window);
        delete app;
    }
    SDL_Quit();
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppContext*>(appstate);
    const auto now = SDL_GetTicks() / 1000.0;
    const float red = static_cast<float>(0.5 + 0.5 * SDL_sin(now));
    const float green = static_cast<float>(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 2 / 3));
    const float blue = static_cast<float>(0.5 + 0.5 * SDL_sin(now + SDL_PI_D * 4 / 3));
    SDL_SetRenderDrawColorFloat(renderer, red, green, blue, SDL_ALPHA_OPAQUE_FLOAT);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}