#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "system.h"

typedef struct {
    uint16_t opcode;
    uint16_t NNN; 
    uint8_t NN; 
    uint8_t N;   
    uint8_t X;   
    uint8_t Y;
} instruction_t;

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

bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]);
void draw_pixel(
    const uint32_t i,
    const uint32_t color,
    const SDL_Rect rect,
    const sdl_t sdl,
    const config_t config,
    chip8_t *chip8
);
void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8);
void handle_input(chip8_t *chip8);
void emulate_instruction(chip8_t *chip8, const config_t config);
void update_timers(const sdl_t sdl, chip8_t *chip8);


#endif