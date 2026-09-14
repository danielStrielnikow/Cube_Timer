#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "driver/i2c_master.h"
#include "ssd1306.h"
#include "wifi_config.h"

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

// ponizej tej wartosci odczyt osi jest traktowany jako szum, a nie grawitacja
#define SIDE_MIN_THRESHOLD 3000

// bok 6 to tryb uspienia kostki - tak samo jak w starym projekcie na Arduino
#define SLEEP_SIDE 6

// minimalny czas miedzy wyslaniami UDP, zeby nie zasypac backendu (1.5s)
#define SEND_COOLDOWN_US (1500 * 1000)

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

static int get_cube_side(int16_t x, int16_t y, int16_t z) {
    int abs_x = abs((int) x);
    int abs_y = abs((int) y);
    int abs_z = abs((int) z);

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


//WIFI
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connect(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Laczenie z WiFi: %s", WIFI_SSID);
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Polaczono z WiFi, wysylam dane do %s:%d", SERVER_IP, SERVER_PORT);
}

//UDP

static int udp_sock = -1;
static struct sockaddr_in server_addr;

static void udp_init(void) {
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Nie udalo sie utworzyc gniazda UDP");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
}

// wysyla do backendu JSON
static void udp_send_side(int side, float battery) {
    if (udp_sock < 0) {
        return;
    }

    char json[96];
    snprintf(json, sizeof(json), "{\"cubeId\":\"%s\",\"side\":%d,\"battery\":%.2f}", CUBE_ID, side, battery);

    int sent = sendto(udp_sock, json, strlen(json), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (sent < 0) {
        ESP_LOGW(TAG, "Nie udalo sie wyslac UDP");
    } else {
        ESP_LOGI(TAG, "UDP wyslano: %s", json);
    }
}

void app_main() {
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    wifi_connect();
    udp_init();

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config_mcu, &dev_handle_mcu));
    ESP_ERROR_CHECK(ssd1306_init(bus_handle, &dev_cfg, &dev_hdl));

    uint8_t wakeUpMCU[] = {MPU6050_REG_PWR_MGMT_1, MPU6050_WAKEUP_DATA};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle_mcu, wakeUpMCU, sizeof(wakeUpMCU), -1));

    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buffer[6];
    int16_t last_x = 0, last_y = 0, last_z = 0;
    bool has_last_reading = false;

    int last_sent_side = -1;
    int64_t last_send_time_us = 0;

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

        int side = get_cube_side(x, y, z);

        if (side != last_sent_side && (now_us - last_send_time_us) >= SEND_COOLDOWN_US) {
            int side_to_send = (side == SLEEP_SIDE) ? 0 : side;
            udp_send_side(side_to_send, 3.85f);
            last_sent_side = side;
            last_send_time_us = now_us;
        }

        // int wystarczy, bez 64-bitowego formatowania w printf
        int elapsed_ms = (int) (get_elapsed_us(now_us) / 1000);

        char lineX[16];
        char lineY[16];
        char lineZ[16];
        char lineStatus[48];

        snprintf(lineX, sizeof(lineX), "X:%-6d", x);
        snprintf(lineY, sizeof(lineY), "Y:%-6d", y);
        snprintf(lineZ, sizeof(lineZ), "Z:%-6d", z);
        if (side == 0) {
            snprintf(lineStatus, sizeof(lineStatus), "Bok:? %d.%02ds",
                     elapsed_ms / 1000, (elapsed_ms % 1000) / 10);
        } else {
            snprintf(lineStatus, sizeof(lineStatus), "Bok:%d %d.%02ds",
                     side, elapsed_ms / 1000, (elapsed_ms % 1000) / 10);
        }

        ssd1306_display_text(dev_hdl, 0, lineX, false);
        ssd1306_display_text(dev_hdl, 1, lineY, false);
        ssd1306_display_text(dev_hdl, 2, lineZ, false);
        ssd1306_display_text(dev_hdl, 3, lineStatus, false);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
