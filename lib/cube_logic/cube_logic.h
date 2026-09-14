#pragma once

#include <stdint.h>

#define SIDE_MIN_THRESHOLD 3000

#define SLEEP_SIDE 6

int get_cube_side(int16_t x, int16_t y, int16_t z);

typedef struct {
    bool running;
    int side;
    int64_t start_time_us;
} cube_timer_t;

void cube_timer_init(cube_timer_t *timer);
void cube_timer_update(cube_timer_t *timer, int side, int64_t now_us);
int64_t cube_timer_elapsed_us(const cube_timer_t *timer, int64_t now_us);
