#include <stdio.h>
#include <stdlib.h> // For rand()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "ugui.h"
#include "gamepad.h"
#include "esp_system.h" // For esp_random()
#include "esp_random.h"

// --- Configuration ---
#define SCREEN_W 320
#define SCREEN_H 240
#define BLOCK_SIZE 10
#define GRID_W (SCREEN_W / BLOCK_SIZE)
#define GRID_H (SCREEN_H / BLOCK_SIZE)
#define MAX_SNAKE_LEN 100

// --- Globals ---
UG_GUI gui;
input_gamepad_state joystick;

typedef struct {
    int x;
    int y;
} Point;

Point snake[MAX_SNAKE_LEN];
int snake_len;
Point food;
int dir_x, dir_y; // Current direction
int score;
bool game_over;

// --- Helper Functions ---

void pixel_set_function(UG_S16 x, UG_S16 y, UG_COLOR color){
    display_draw_pixel(x, y, color);
}

// Draw a block on the grid
void draw_block(int x, int y, UG_COLOR color) {
    int px = x * BLOCK_SIZE;
    int py = y * BLOCK_SIZE;
    // Leave a 1px border so blocks look distinct
    UG_FillFrame(px, py, px + BLOCK_SIZE - 2, py + BLOCK_SIZE - 2, color);
}

void spawn_food() {
    // Keep trying until we find a spot not occupied by snake
    bool valid = false;
    while (!valid) {
        valid = true;
        food.x = esp_random() % GRID_W;
        food.y = esp_random() % GRID_H;
        
        // Check collision with snake
        for(int i=0; i<snake_len; i++) {
            if(snake[i].x == food.x && snake[i].y == food.y) {
                valid = false;
                break;
            }
        }
    }
    draw_block(food.x, food.y, C_RED);
}

void reset_game() {
    game_over = false;
    score = 0;
    snake_len = 3;
    dir_x = 1; // Start moving Right
    dir_y = 0;
    
    // Center the snake
    snake[0].x = GRID_W / 2;
    snake[0].y = GRID_H / 2;
    snake[1].x = snake[0].x - 1; snake[1].y = snake[0].y;
    snake[2].x = snake[0].x - 2; snake[2].y = snake[0].y;
    
    UG_FillScreen(C_BLACK);
    
    // Draw Border
    UG_DrawFrame(0, 0, SCREEN_W-1, SCREEN_H-1, C_BLUE);
    
    // Draw initial snake
    for(int i=0; i<snake_len; i++) {
        draw_block(snake[i].x, snake[i].y, C_WHITE);
    }
    spawn_food();
}

// --- Main ---

void app_main(void)
{
    display_init();
    gamepad_init();
    
    UG_Init(&gui, pixel_set_function, 320, 240);
    UG_SelectGUI(&gui);
    UG_FontSelect(&FONT_12X20); // Large font for Game Over
    
    reset_game();

    while(1) {
        gamepad_read(&joystick);

        if (game_over) {
            // Wait for Start or A to restart
            if (joystick.values[GAMEPAD_INPUT_START] || joystick.values[GAMEPAD_INPUT_A]) {
                reset_game();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // --- 1. Input Handling ---
        // Prevent 180 degree turns (can't go Left if moving Right)
        if (joystick.values[GAMEPAD_INPUT_UP]    && dir_y == 0) { dir_x = 0; dir_y = -1; }
        if (joystick.values[GAMEPAD_INPUT_DOWN]  && dir_y == 0) { dir_x = 0; dir_y = 1; }
        if (joystick.values[GAMEPAD_INPUT_LEFT]  && dir_x == 0) { dir_x = -1; dir_y = 0; }
        if (joystick.values[GAMEPAD_INPUT_RIGHT] && dir_x == 0) { dir_x = 1; dir_y = 0; }

        // --- 2. Calculate New Head Position ---
        Point new_head = { snake[0].x + dir_x, snake[0].y + dir_y };

        // --- 3. Collision Checks ---
        // Wall Collision
        if (new_head.x < 0 || new_head.x >= GRID_W || new_head.y < 0 || new_head.y >= GRID_H) {
            game_over = true;
        }
        
        // Self Collision
        for(int i=0; i<snake_len; i++) {
            if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
                game_over = true;
            }
        }

        if (game_over) {
            UG_SetForecolor(C_RED);
            UG_SetBackcolor(C_BLACK);
            UG_PutString(110, 100, "GAME OVER");
            UG_SetForecolor(C_WHITE);
            UG_PutString(90, 130, "Press Start");
            continue; // Skip rest of loop
        }

        // --- 4. Move Snake ---
        
        // Check if we ate food
        bool ate = (new_head.x == food.x && new_head.y == food.y);
        
        if (ate) {
            score++;
            if(snake_len < MAX_SNAKE_LEN) snake_len++;
            spawn_food();
            // Don't erase tail this frame (it grows!)
        } else {
            // Erase the old tail
            Point tail = snake[snake_len-1];
            draw_block(tail.x, tail.y, C_BLACK); 
        }

        // Shift Body
        for(int i = snake_len-1; i > 0; i--) {
            snake[i] = snake[i-1];
        }
        
        // Update Head
        snake[0] = new_head;
        draw_block(snake[0].x, snake[0].y, C_GREEN); // Head is Green
        if(snake_len > 1) draw_block(snake[1].x, snake[1].y, C_WHITE); // Body becomes White

        // --- 5. Speed Control ---
        vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms = 10 FPS
    }
}
