#include <stdio.h>
#include <stdint.h>
#include "system.h"
#include "chip8.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
       fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
       exit(EXIT_FAILURE);
    }

    config_t config = {0};
    if(!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

    sdl_t sdl = {0};
    if(!init_sdl(&sdl, &config)) exit(EXIT_FAILURE);

    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if(!init_chip8(&chip8, config, rom_name)) exit(EXIT_FAILURE);

    clear_screen(sdl, config);

    while(chip8.state != QUIT){
        handle_input(&chip8);

        if(chip8.state == PAUSED) continue;

        const uint64_t start_frame_time = SDL_GetPerformanceCounter();
        for (uint32_t i = 0; i < config.insts_per_second / 60; i++) {
            emulate_instruction(&chip8, config);

            if (chip8.inst.opcode >> 12 == 0xD) break;
        }
        const uint64_t end_frame_time = SDL_GetPerformanceCounter();

        const double time_elapsed = (double)((end_frame_time - start_frame_time) * 1000) / SDL_GetPerformanceFrequency();

        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

        if(chip8.draw){
            update_screen(sdl, config, &chip8);
            chip8.draw = false;
        }

        update_timers(sdl, &chip8);
    }

    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}
