#include "system.h"

bool set_config_from_args(config_t *config, const int argc, char **argv){
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF,
        .bg_color = 0x000000FF,
        .scale_factor = 20,
        .insts_per_second = 600,
        .square_wave_freq = 440,
        .audio_sample_rate = 44100,
        .volume = 3000, 
    };

    for(int i = 0; i < argc; i++){
        argv[i];
    }

    return true;
};

bool init_sdl(sdl_t *sdl, config_t *config){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
        SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow(
        "CHIP8 Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config->window_width * config->scale_factor,
        config->window_height * config->scale_factor,
        0
    );

    if(!sdl->window){
        SDL_Log("Could not create SDL window! %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(
        sdl->window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if(!sdl->renderer){
        SDL_Log("Could not create SDL renderer! %s\n", SDL_GetError());
        return false;
    }

    sdl->want = (SDL_AudioSpec){
        .freq = 44100,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .samples = 512,
        .callback = audio_callback,
        .userdata = config,
    };

    sdl->dev = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);

    if (sdl->dev == 0) {
        SDL_Log("Could not get an Audio Device %s\n", SDL_GetError());
        return false;
    }

    if ((sdl->want.format != sdl->have.format) ||
        (sdl->want.channels != sdl->have.channels)) {

        SDL_Log("Could not get desired Audio Spec\n");
        return false;
    }

    return true;
}

void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_CloseAudioDevice(sdl.dev);
    SDL_Quit();
}

void audio_callback(void *userdata, uint8_t *stream, int len) {
    config_t *config = (config_t *)userdata;

    int16_t *audio_data = (int16_t *)stream;
    static uint32_t running_sample_index = 0;
    const int32_t square_wave_period = config->audio_sample_rate / config->square_wave_freq;
    const int32_t half_square_wave_period = square_wave_period / 2;

    for (int i = 0; i < len / 2; i++) {
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ? 
                        config->volume :
                        -config->volume;
    }
}