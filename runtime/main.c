#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"
#include "cpu.h"
#include "input.h"
#include "ppu.h"

void register_dispatch(void (*dispatch)(uint16_t addr));
#ifdef NES_RECOMPILED
void game_main(void);
#endif

static void handle_key(const SDL_Event *event, int pressed)
{
    SDL_Keycode key = event->key.keysym.sym;
    unsigned button = 8;
    if (key == SDLK_z) button = 0;
    else if (key == SDLK_x) button = 1;
    else if (key == SDLK_RSHIFT) button = 2;
    else if (key == SDLK_RETURN) button = 3;
    else if (key == SDLK_UP) button = 4;
    else if (key == SDLK_DOWN) button = 5;
    else if (key == SDLK_LEFT) button = 6;
    else if (key == SDLK_RIGHT) button = 7;
    if (button < 8) input_set_button(button, pressed);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !bus_load_prg(argv[1], 2)) return 2;
    int screenshot_frame = -1;
    const char *screenshot_path = NULL;
    unsigned ppu_log_frames = 0;
    unsigned ppu_dump_frame = 0;
    unsigned ppm_frame = 0;
    const char *ppm_path = NULL;
    unsigned scroll_log_frame = 0;
    int frame_log = 0;
    int bg_log = 0;
    int test_gradient = 0;
    int recompiled = 0;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--screenshot-frame") == 0 && i + 2 < argc) {
            screenshot_frame = atoi(argv[++i]);
            screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--ppu-write-log") == 0) {
            ppu_log_frames = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ppu-dump-frame") == 0) {
            ppu_dump_frame = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--test-gradient") == 0) {
            test_gradient = 1;
            if (i + 1 < argc) screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--dump-ppm") == 0 && i + 2 < argc) {
            ppm_frame = (unsigned)atoi(argv[++i]);
            ppm_path = argv[++i];
        } else if (strcmp(argv[i], "--scroll-log") == 0) {
            scroll_log_frame = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bg-log") == 0) {
            bg_log = 1;
        } else if (strcmp(argv[i], "--frame-log") == 0) {
            frame_log = 1;
        } else if (strcmp(argv[i], "--recompiled") == 0) {
            recompiled = 1;
        }
    }
    ppu_set_write_log_frames(ppu_log_frames);
    if (ppu_dump_frame != 0) ppu_set_nametable_dump_frame(ppu_dump_frame);
    if (ppm_frame != 0) ppu_set_ppm_dump(ppm_frame, ppm_path);
    if (scroll_log_frame != 0) ppu_set_scroll_log_frame(scroll_log_frame);
    ppu_set_bg_log(bg_log);
    ppu_set_frame_log(frame_log);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("NES Static Recompiler",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, NES_W * 2, NES_H * 2, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (window != NULL && renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, NES_W, NES_H);
    SDL_Texture *texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, NES_W, NES_H) : NULL;
    if (texture == NULL) {
        fprintf(stderr, "SDL texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    int running = 1;
    #ifdef NES_RECOMPILED
    if (recompiled) game_main();
    #else
    recompiled = 0;
    #endif
    cpu.pc = (uint16_t)(bus_read(0xfffc) | (bus_read(0xfffd) << 8));
    cpu.sp = 0xfd;
    int frame_count = 0;
    if (test_gradient) {
        ppu_fill_test_gradient();
        SDL_UpdateTexture(texture, NULL, framebuffer, NES_W * sizeof(uint32_t));
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        if (screenshot_path != NULL) {
            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
                framebuffer, NES_W, NES_H, 32, NES_W * sizeof(uint32_t),
                0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            if (surface != NULL) {
                SDL_SaveBMP(surface, screenshot_path);
                SDL_FreeSurface(surface);
            }
        }
        running = 0;
    }
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            else if (event.type == SDL_KEYDOWN) handle_key(&event, 1);
            else if (event.type == SDL_KEYUP) handle_key(&event, 0);
        }
        for (unsigned cycle = 0; cycle < 90000 && running; ++cycle) {
            if (recompiled) call_addr(cpu.pc);
            else cpu_step();
            ppu_step(1);
            cpu_safe_point();
        }
        if (ppu_frame_ready()) {
            SDL_UpdateTexture(texture, NULL, framebuffer, NES_W * sizeof(uint32_t));
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            ppu_clear_frame_ready();
            if (ppu_ppm_dump_complete() && screenshot_frame < 0) running = 0;
            ++frame_count;
            if (screenshot_frame == frame_count && screenshot_path != NULL) {
                SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
                    framebuffer, NES_W, NES_H, 32, NES_W * sizeof(uint32_t),
                    0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                if (surface != NULL) {
                    SDL_SaveBMP(surface, screenshot_path);
                    SDL_FreeSurface(surface);
                }
                running = 0;
            }
        }
        SDL_Delay(16);
    }
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return running ? 0 : 0;
}
