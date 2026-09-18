#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include "esp_sleep.h"
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "driver/i2c_master.h"
#include "wifi_config.h"
#include "mqtt_config.h"
#include "cube_logic.h"
#include "mqtt_client.h"

static const char *TAG = "Cube_Timer";

#define PIN_I2C_SCL GPIO_NUM_22
#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_MPU_INT GPIO_NUM_33

const uint8_t MPU6050_ADDR = 0x68;
const uint8_t SSD1306_ADDR = 0x3C;
const uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;

// Zmienne w RTC SRAM – zachowują stan po wybudzeniu z Deep Sleep
static RTC_DATA_ATTR int64_t last_wakeup_us = 0;
static RTC_DATA_ATTR uint32_t boot_count = 0;
static RTC_DATA_ATTR int current_cube_side = -1;

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

i2c_device_config_t dev_config_mcu = {
    .device_address = MPU6050_ADDR,
    .scl_speed_hz = 400000,
};



//WIFI
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static volatile bool wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connect_and_sync(int side, double duration_s) {
    // Łączymy się TYLKO gdy trzeba nadać raport MQTT
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Laczenie z Wi-Fi do wyslania raportu...");
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));

    if (bits & WIFI_CONNECTED_BIT) {
        // Konfiguracja MQTT
        const esp_mqtt_client_config_t mqtt_cfg = {
            .broker = { .address.uri = ADDRESS_URI, .address.port = ADDRESS_PORT },
        };
        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_start(client);

        // Formatowanie i publikacja
        char topic[64];
        char json[128];
        snprintf(topic, sizeof(topic), "cube/%s/session", CUBE_ID);
        snprintf(json, sizeof(json), "{\"cubeId\":\"%s\",\"side\":%d,\"duration_s\":%.2f}", CUBE_ID, side, duration_s);

        // Czekamy chwilę na połączenie z brokerem i publikujemy
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_mqtt_client_publish(client, topic, json, 0, 1, 0);
        ESP_LOGI(TAG, "Wyslano MQTT: %s", json);

        vTaskDelay(pdMS_TO_TICKS(500));
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    } else {
        ESP_LOGW(TAG, "Timeout Wi-Fi, pomijam wysylke MQTT.");
    }

    esp_wifi_stop();
}

static void mpu6050_configure_motion_interrupt(i2c_master_dev_handle_t dev) {
#define MPU_WRITE(reg, val) do { \
        uint8_t d[2] = { (reg), (val) }; \
        ESP_ERROR_CHECK(i2c_master_transmit(dev, d, sizeof(d), -1)); \
    } while(0)

    MPU_WRITE(0x6B, 0x00); // Wybudzenie MPU6050
    MPU_WRITE(0x1C, 0x01); // Włączenie filtra DHPF
    MPU_WRITE(0x1F, 25);   // Próg czułości ruchu (MOT_THR)
    MPU_WRITE(0x20, 2);    // Czas trwania ruchu (MOT_DUR)
    MPU_WRITE(0x37, 0x20); // Pin INT: aktywny wysoki, zatrzask (latch)
    MPU_WRITE(0x38, 0x40); // Włączenie przerwania Motion Detection

#undef MPU_WRITE
}

static void mpu6050_clear_interrupt(i2c_master_dev_handle_t dev) {
    uint8_t reg = 0x3A; // Rejestr INT_STATUS
    uint8_t status = 0;
    i2c_master_transmit_receive(dev, &reg, 1, &status, 1, -1);
}

void app_main(void) {
    int64_t now_us = esp_timer_get_time();
    boot_count++;

    // Sprawdzenie przyczyny wybudzenia
    uint32_t wakeup_cause = esp_sleep_get_wakeup_causes();

    // Inicjalizacja magistrali I2C i peryferiów
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config_mcu, &dev_handle_mcu));

    // Odczyt akcelerometru
    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buffer[6];
    int new_side = -1;

    if (i2c_master_transmit_receive(dev_handle_mcu, &reg, sizeof(reg), buffer, sizeof(buffer), -1) == ESP_OK) {
        int16_t x = (buffer[0] << 8) | buffer[1];
        int16_t y = (buffer[2] << 8) | buffer[3];
        int16_t z = (buffer[4] << 8) | buffer[5];
        new_side = get_cube_side(x, y, z);
    }

    if (wakeup_cause & ESP_SLEEP_WAKEUP_EXT0) {
        int64_t diff_us = now_us - last_wakeup_us;
        double diff_seconds = (double)diff_us / 1000000.0;

        ESP_LOGI(TAG, "Kostka poruszona! Byla na boku %d przez %.2f s", current_cube_side, diff_seconds);

        if (new_side != current_cube_side && current_cube_side != -1) {
            esp_err_t nvs_err = nvs_flash_init();
            if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                nvs_err = nvs_flash_init();
            }
            ESP_ERROR_CHECK(nvs_err);

            wifi_connect_and_sync(current_cube_side, diff_seconds);
        }
    } else {
        ESP_LOGI(TAG, "Pierwszy start urzadzenia.");
    }

    last_wakeup_us = now_us;
    current_cube_side = new_side;

    mpu6050_configure_motion_interrupt(dev_handle_mcu);
    mpu6050_clear_interrupt(dev_handle_mcu);

    esp_sleep_enable_ext0_wakeup(PIN_MPU_INT, 1);

    ESP_LOGI(TAG, "Przejscie w Deep Sleep. Oczekiwanie na ruch kostki...");

    esp_deep_sleep_start();
}