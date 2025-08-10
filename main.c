#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
} sdl_t;

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
    uint16_t opcode;
    uint16_t NNN; 
    uint8_t NN; 
    uint8_t N;   
    uint8_t X;   
    uint8_t Y;
} instruction_t;

typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64*32];
    uint32_t pixel_color[64*32];
    uint16_t stack[12];
    uint16_t *stack_ptr;
    uint8_t V[16];
    uint16_t PC;
    uint16_t I; 
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[16];
    const char *rom_name; 
    instruction_t inst;
    bool draw;
} chip8_t;

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

bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]){
    const uint32_t entry_point = 0x200;
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };

    memset(chip8, 0, sizeof(chip8_t));

    memcpy(&chip8->ram[0], font, sizeof(font));
   
    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        SDL_Log("Rom file %s is invalid or does not exist\n", rom_name);
        return false;
    }

    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_Log("Rom file %s is too big! Rom size: %llu, Max size allowed: %llu\n", 
                rom_name, (long long unsigned)rom_size, (long long unsigned)max_size);
        return false;
    }

    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read Rom file %s into CHIP8 memory\n", 
                rom_name);
        return false;
    }
    fclose(rom);

    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];
    memset(&chip8->pixel_color[0], config.bg_color, sizeof chip8->pixel_color);

    return true;
}

void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_CloseAudioDevice(sdl.dev);
    SDL_Quit();
}

void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void draw_pixel(
    const uint32_t i,
    const uint32_t color,
    const SDL_Rect rect,
    const sdl_t sdl,
    const config_t config,
    chip8_t *chip8
){
            chip8->pixel_color[i] = color;
            const uint8_t r = (chip8->pixel_color[i] >> 24) & 0xFF;
            const uint8_t g = (chip8->pixel_color[i] >> 16) & 0xFF;
            const uint8_t b = (chip8->pixel_color[i] >>  8) & 0xFF;
            const uint8_t a = (chip8->pixel_color[i] >>  0) & 0xFF;

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);
}

void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8){
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    for (uint32_t i = 0; i < sizeof chip8->display; i++) {
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8->display[i]) {
            draw_pixel(i, config.fg_color, rect, sdl, config, chip8);
            continue;
        }

        draw_pixel(i, config.bg_color, rect, sdl, config, chip8);
    }

    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch (event.type)
        {
            case SDL_QUIT:
                chip8->state = QUIT;
                return;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;

                    case SDLK_SPACE:
                        if(chip8->state == RUNNING){
                            chip8->state = PAUSED;
                            puts("PAUSED");
                            return;
                        }

                        chip8->state = RUNNING;
                        return;

                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;

                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;

                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;

                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;
                        
                    default:
                        break;
                }
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;

                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;

                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;

                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;

                    default: break;
                }
                break;

            default:
                break;
        }
    }
}

void emulate_instruction(chip8_t *chip8, const config_t config){
    bool carry;

    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1]; //16bits
    chip8->PC += 2; //read 2 byte for time or 16 bits

    //smaller part of the opcode
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF; //last 12 bits = memory address
    chip8->inst.NN = chip8->inst.opcode & 0x0FF; // last 8 bits = value
    chip8->inst.N = chip8->inst.opcode & 0x0F; // last 4 bits = value
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F; // = register(V) flag value
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F; // = register(V) flag value

    const uint8_t inst = (chip8->inst.opcode >> 12) & 0x0F; //first 4 bits = instruction

    switch (inst) {
        case 0x00:
            if (chip8->inst.NN == 0xE0) { //clear screen
                memset(&chip8->display[0], false, sizeof chip8->display);
                chip8->draw = true;
                printf("Clear screen\n");
                break;
            } 
            
            if(chip8->inst.NN == 0xEE){ //subroutines
                chip8->PC = *--chip8->stack_ptr;
                break;
            }
            break;

        case 0x01: //jump
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02: //subroutines
            *chip8->stack_ptr++ = chip8->PC;  
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03: //jump if
            if(chip8->V[chip8->inst.X] == chip8->inst.NN){
                chip8->PC += 2;
            }
            break;

        case 0x04: //jump not if
            if(chip8->V[chip8->inst.X] != chip8->inst.NN){
                chip8->PC += 2;
            }
            break;

        case 0x05: //jump if
            if (chip8->inst.N != 0) break; //invalid opcode

            if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
                chip8->PC += 2;
            }
            break;

        case 0x06: //set register(V)
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07: //add register(V)
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08: //math
            switch (chip8->inst.N) //last 4 bits
            {
                case 0x00:
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    break;

                case 0x01:
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    break;

                case 0x02:
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    break;

                case 0x03:
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    break;
                    
                case 0x04:
                    carry = ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);

                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;

                case 0x05:
                    carry = chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y];

                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;

                case 0x06:
                    carry = chip8->V[chip8->inst.Y] & 1;

                    chip8->V[chip8->inst.X] >>= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;

                case 0x07:
                    carry = chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y];

                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = carry;
                    break;

                case 0x0E:
                    carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;

                    chip8->V[chip8->inst.X] <<= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry;
                    break;
                
                default:
                    break;
            }
            break;

        case 0x09: //jump if
            if (chip8->inst.N != 0) break; //invalid opcode

            if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){
                chip8->PC += 2;
            }
            break;

        case 0x0A: //set register(I)
            chip8->I = chip8->inst.NNN;
            break;

        case 0x0B: //set register(PC)
            chip8->PC = chip8->V[0] + chip8->inst.NNN;
            break;

        case 0x0C: //set register(V)
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;

        case 0x0D: //draw screen
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_coord;

            chip8->V[0xF] = 0; 

            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X;

                for (int8_t j = 7; j >= 0; j--) {
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord]; 
                    const bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;  
                    }

                    *pixel ^= sprite_bit;

                    if (++X_coord >= config.window_width) break;
                }

                if (++Y_coord >= config.window_height) break;
            }
            chip8->draw = true;
            break;

        case 0x0E: //set register(PC)
            if (chip8->inst.NN == 0x9E) {
                if (chip8->keypad[chip8->V[chip8->inst.X]]) chip8->PC += 2;
                break;
            } 
            
            if (chip8->inst.NN == 0xA1) {
                if (!chip8->keypad[chip8->V[chip8->inst.X]]) chip8->PC += 2;
                break;
            }
            break;

        case 0x0F:
                switch (chip8->inst.NN) {
                    case 0x07: //set register(V)
                        chip8->V[chip8->inst.X] = chip8->delay_timer;
                        break;

                    case 0x0A: //set register(PC) or register(V)
                        static bool any_key_pressed = false;
                        static uint8_t key = 0xFF;

                        for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypad; i++){
                            if (chip8->keypad[i]) {
                                key = i;
                                any_key_pressed = true;
                                break;
                            }
                        }

                        if (!any_key_pressed){
                            chip8->PC -= 2;
                            break;
                        }

                        if (chip8->keypad[key]){
                            chip8->PC -= 2;
                            break;
                        }
                            
                        chip8->V[chip8->inst.X] = key;
                        key = 0xFF;
                        any_key_pressed = false;
                        break;

                    case 0x15: //set delay_timer
                        chip8->delay_timer = chip8->V[chip8->inst.X];
                        break;

                    case 0x18: //set sound_timer
                        chip8->sound_timer = chip8->V[chip8->inst.X];
                        break;

                    case 0x1E: //set register(I)
                        chip8->I += chip8->V[chip8->inst.X];
                        break;

                    case 0x29: //set register(I) sprite
                        chip8->I = chip8->V[chip8->inst.X] * 5;
                        break;

                    case 0x33:
                        uint8_t bcd = chip8->V[chip8->inst.X]; 
                        chip8->ram[chip8->I+2] = bcd % 10;
                        bcd /= 10;
                        chip8->ram[chip8->I+1] = bcd % 10;
                        bcd /= 10;
                        chip8->ram[chip8->I] = bcd;
                        break;

                    case 0x55:
                        for (uint8_t i = 0; i <= chip8->inst.X; i++)  {
                            chip8->ram[chip8->I++] = chip8->V[i];
                        }
                        break;

                    case 0x65:
                        for (uint8_t i = 0; i <= chip8->inst.X; i++) {
                            chip8->V[i] = chip8->ram[chip8->I++];
                        }
                        break;

                    default: //opcode invalid
                        break;
                }
            break;

        default: //opcode invalid
            break;
    }
}

void update_timers(const sdl_t sdl, chip8_t *chip8) {
    if (chip8->delay_timer > 0) 
        chip8->delay_timer--;

    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl.dev, 0);
        return;
    }

    SDL_PauseAudioDevice(sdl.dev, 1);
}

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
