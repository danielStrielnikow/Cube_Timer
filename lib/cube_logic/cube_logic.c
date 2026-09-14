#include "cube_logic.h"

#include <stdlib.h>

int get_cube_side(int16_t x, int16_t y, int16_t z) {
    int abs_x = abs(x);
    int abs_y = abs(y);
    int abs_z = abs(z);

    if (abs_x < SIDE_MIN_THRESHOLD && abs_y < SIDE_MIN_THRESHOLD && abs_z < SIDE_MIN_THRESHOLD) {
        return 0;
    }

    if (abs_z >= abs_x && abs_z >= abs_y) {
        return z > 0 ? 1 : 2;
    }
    if (abs_x >= abs_y) {
        return x > 0 ? 3 : 4;
    }
    return y > 0 ? 5 : 6;
}

void cube_timer_init(cube_timer_t *timer) {
    timer->running = false;
    timer->side = 0;
    timer->start_time_us = 0;
}

void cube_timer_update(cube_timer_t *timer, int side, int64_t now_us) {
    bool is_work_side = side >= 1 && side <= 5;

    if (!is_work_side) {
        timer->running = false;
        return;
    }

    if (!timer->running || side != timer->side) {
        timer->side = side;
        timer->start_time_us = now_us;
        timer->running = true;
    }
}

int64_t cube_timer_elapsed_us(const cube_timer_t *timer, int64_t now_us) {
    if (!timer->running) {
        return 0;
    }
    return now_us - timer->start_time_us;
}
