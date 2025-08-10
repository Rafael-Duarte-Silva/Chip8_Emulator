#ifndef SYTEM_H
#define SYTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

typedef struct {
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_factor;
    uint32_t insts_per_second;
    uint32_t square_wave_freq;
    uint32_t audio_sample_rate; 
    int16_t volume; 
} config_t;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
} sdl_t;

typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

bool set_config_from_args(config_t *config, const int argc, char **argv);
bool init_sdl(sdl_t *sdl, config_t *config);
void clear_screen(const sdl_t sdl, const config_t config);
void final_cleanup(const sdl_t sdl);
void audio_callback(void *userdata, uint8_t *stream, int len);

#endif