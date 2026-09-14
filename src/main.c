#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "ssd1306.h"

static const char *TAG = "Cube_Timer";

#define PIN_I2C_SCL GPIO_NUM_22
#define PIN_I2C_SDA GPIO_NUM_21

const uint8_t MPU6050_ADDR = 0x68;
const uint8_t SSD1306_ADDR = 0x3C;

const uint8_t MPU6050_REG_PWR_MGMT_1 = 0x6B;
const uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU6050_WAKEUP_DATA = 0x00;

// prog roznicy odczytu, powyzej ktorego uznajemy to za ruch kostki
#define MOVEMENT_THRESHOLD 3000

// ile czasu bez ruchu oznacza, ze kostka zostala odlozona
#define STILL_TIME_TO_STOP_US (1000 * 1000)

static i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,
    .scl_io_num = PIN_I2C_SCL,
    .sda_io_num = PIN_I2C_SDA,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle_mcu;
static ssd1306_config_t dev_cfg = SSD1306_128x32_CONFIG_DEFAULT;
static ssd1306_handle_t dev_hdl;

i2c_device_config_t dev_config_mcu = {
    .device_address = MPU6050_ADDR,
    .scl_speed_hz = 400000,
};

typedef enum {
    TIMER_STATE_WAITING,
    TIMER_STATE_RUNNING,
    TIMER_STATE_FINISHED,
} timer_state_t;

static timer_state_t timer_state = TIMER_STATE_WAITING;
static int64_t start_time_us = 0;
static int64_t stop_time_us = 0;
static int64_t still_since_us = 0;

static void update_timer(bool moving, int64_t now_us) {
    switch (timer_state) {
        case TIMER_STATE_WAITING:
            if (moving) {
                start_time_us = now_us;
                still_since_us = 0;
                timer_state = TIMER_STATE_RUNNING;
            }
            break;

        case TIMER_STATE_RUNNING:
            if (moving) {
                still_since_us = 0;
            } else if (still_since_us == 0) {
                still_since_us = now_us;
            } else if (now_us - still_since_us > STILL_TIME_TO_STOP_US) {
                stop_time_us = still_since_us;
                timer_state = TIMER_STATE_FINISHED;
            }
            break;

        case TIMER_STATE_FINISHED:
            if (moving) {
                start_time_us = now_us;
                still_since_us = 0;
                timer_state = TIMER_STATE_RUNNING;
            }
            break;
    }
}

static int64_t get_elapsed_us(int64_t now_us) {
    switch (timer_state) {
        case TIMER_STATE_RUNNING:
            return now_us - start_time_us;
        case TIMER_STATE_FINISHED:
            return stop_time_us - start_time_us;
        default:
            return 0;
    }
}

void app_main() {
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config_mcu, &dev_handle_mcu));
    ESP_ERROR_CHECK(ssd1306_init(bus_handle, &dev_cfg, &dev_hdl));

    uint8_t wakeUpMCU[] = {MPU6050_REG_PWR_MGMT_1, MPU6050_WAKEUP_DATA};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle_mcu, wakeUpMCU, sizeof(wakeUpMCU), -1));

    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buffer[6];
    int16_t last_x = 0, last_y = 0, last_z = 0;
    bool has_last_reading = false;

    ESP_LOGI(TAG, "######################## Cube Timer - START #########################");
    ssd1306_clear_display(dev_hdl, false);

    while (1) {
        esp_err_t err = i2c_master_transmit_receive(dev_handle_mcu, &reg, sizeof(reg), buffer, sizeof(buffer), -1);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Nie udalo sie odczytac danych z czujnika: %s", esp_err_to_name(err));
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        int16_t x = (buffer[0] << 8) | buffer[1];
        int16_t y = (buffer[2] << 8) | buffer[3];
        int16_t z = (buffer[4] << 8) | buffer[5];

        int32_t diff = 0;
        if (has_last_reading) {
            diff = abs(x - last_x) + abs(y - last_y) + abs(z - last_z);
        }
        has_last_reading = true;
        last_x = x;
        last_y = y;
        last_z = z;

        bool moving = diff > MOVEMENT_THRESHOLD;
        int64_t now_us = esp_timer_get_time();
        update_timer(moving, now_us);

        // int wystarczy, bez 64-bitowego formatowania w printf
        int elapsed_ms = (int) (get_elapsed_us(now_us) / 1000);

        char lineX[16];
        char lineY[16];
        char lineZ[16];
        char lineTime[16];

        snprintf(lineX, sizeof(lineX), "X:%-6d", x);
        snprintf(lineY, sizeof(lineY), "Y:%-6d", y);
        snprintf(lineZ, sizeof(lineZ), "Z:%-6d", z);
        snprintf(lineTime, sizeof(lineTime), "Czas:%d.%02ds",
                 elapsed_ms / 1000, (elapsed_ms % 1000) / 10);

        ssd1306_display_text(dev_hdl, 0, lineX, false);
        ssd1306_display_text(dev_hdl, 1, lineY, false);
        ssd1306_display_text(dev_hdl, 2, lineZ, false);
        ssd1306_display_text(dev_hdl, 3, lineTime, false);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
