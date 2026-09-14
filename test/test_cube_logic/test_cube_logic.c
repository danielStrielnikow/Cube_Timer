#include <unity.h>
#include "cube_logic.h"

void setUp(void) {}
void tearDown(void) {}


static void test_side_z_plus(void) {
    TEST_ASSERT_EQUAL_INT(1, get_cube_side(0, 0, 16000));
}

static void test_side_z_minus(void) {
    TEST_ASSERT_EQUAL_INT(2, get_cube_side(0, 0, -16000));
}

static void test_side_x_plus(void) {
    TEST_ASSERT_EQUAL_INT(3, get_cube_side(16000, 0, 0));
}

static void test_side_x_minus(void) {
    TEST_ASSERT_EQUAL_INT(4, get_cube_side(-16000, 0, 0));
}

static void test_side_y_plus(void) {
    TEST_ASSERT_EQUAL_INT(5, get_cube_side(0, 16000, 0));
}

static void test_side_y_minus(void) {
    TEST_ASSERT_EQUAL_INT(6, get_cube_side(0, -16000, 0));
}

static void test_side_unknown_when_all_below_threshold(void) {
    // kostka w ruchu / pod skosem - zaden odczyt nie jest wystarczajaco duzy
    TEST_ASSERT_EQUAL_INT(0, get_cube_side(100, -200, 500));
}

static void test_side_exactly_at_threshold_counts_as_valid(void) {
    // SIDE_MIN_THRESHOLD samo w sobie NIE jest jeszcze szumem
    TEST_ASSERT_EQUAL_INT(1, get_cube_side(0, 0, SIDE_MIN_THRESHOLD));
}

static void test_side_tie_prefers_z_axis(void) {
    // przy remisie miedzy osiami, kod sprawdza najpierw Z
    TEST_ASSERT_EQUAL_INT(1, get_cube_side(16000, 16000, 16000));
}



static void test_timer_starts_not_running(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 5000));
}

static void test_timer_stays_stopped_on_unknown_side(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 0, 1000);

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 1000));
}

static void test_timer_starts_on_work_side(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 3, 1000);

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 1000));
    TEST_ASSERT_EQUAL_INT64(4000, cube_timer_elapsed_us(&timer, 5000));
}

static void test_timer_keeps_counting_on_same_side(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 3, 1000);
    // kolejne odczyty tego samego boku nie moga zerowac stopera
    cube_timer_update(&timer, 3, 2000);
    cube_timer_update(&timer, 3, 3000);

    TEST_ASSERT_EQUAL_INT64(2000, cube_timer_elapsed_us(&timer, 3000));
}

static void test_timer_resets_on_side_change(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 3, 1000);
    cube_timer_update(&timer, 3, 5000); // 4000us na boku 3

    cube_timer_update(&timer, 5, 5000); // zmiana boku - restart

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 5000));
    TEST_ASSERT_EQUAL_INT64(1000, cube_timer_elapsed_us(&timer, 6000));
}

static void test_timer_stops_on_sleep_side(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 3, 1000);
    cube_timer_update(&timer, SLEEP_SIDE, 5000);

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 9000));
}

static void test_timer_restarts_after_sleep(void) {
    cube_timer_t timer;
    cube_timer_init(&timer);

    cube_timer_update(&timer, 3, 1000);
    cube_timer_update(&timer, SLEEP_SIDE, 5000);
    cube_timer_update(&timer, 2, 9000);

    TEST_ASSERT_EQUAL_INT64(0, cube_timer_elapsed_us(&timer, 9000));
    TEST_ASSERT_EQUAL_INT64(500, cube_timer_elapsed_us(&timer, 9500));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_side_z_plus);
    RUN_TEST(test_side_z_minus);
    RUN_TEST(test_side_x_plus);
    RUN_TEST(test_side_x_minus);
    RUN_TEST(test_side_y_plus);
    RUN_TEST(test_side_y_minus);
    RUN_TEST(test_side_unknown_when_all_below_threshold);
    RUN_TEST(test_side_exactly_at_threshold_counts_as_valid);
    RUN_TEST(test_side_tie_prefers_z_axis);

    RUN_TEST(test_timer_starts_not_running);
    RUN_TEST(test_timer_stays_stopped_on_unknown_side);
    RUN_TEST(test_timer_starts_on_work_side);
    RUN_TEST(test_timer_keeps_counting_on_same_side);
    RUN_TEST(test_timer_resets_on_side_change);
    RUN_TEST(test_timer_stops_on_sleep_side);
    RUN_TEST(test_timer_restarts_after_sleep);

    return UNITY_END();
}
