/*
 * @Author: Avidel_Arch 1215935448@qq.com
 * @Date: 2025-03-17 00:03:06
 * @LastEditors: Avidel_Arch 1215935448@qq.com
 * @LastEditTime: 2025-03-23 13:21:02
 * @FilePath: /MiniRender/src/main.cpp
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#include <SDL3/SDL_init.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

 struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* message_tex, *image_tex;
    SDL_FRect message_dest;
    SDL_AudioDeviceID audio_device;
    SDL_AppResult app_quit = SDL_APP_CONTINUE;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if(!SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Log("Hello, SDL3!");
    SDL_Window* window(SDL_CreateWindow("Hello, SDL3!", 800, 600, SDL_WINDOW_RESIZABLE));
    if(window == nullptr){
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_Renderer* renderer(SDL_CreateRenderer(window, nullptr));
    if(renderer == nullptr){
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Event event;
    bool keep_going = true;
    while (keep_going){
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_EVENT_QUIT){
                keep_going = false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 10, 20, 30, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    auto* app = (AppContext*)appstate;
    if(event->type == SDL_EVENT_QUIT)
    {
        app->app_quit = SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SDL_Quit();
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto* app = (AppContext*)appstate;
    return app->app_quit;
}