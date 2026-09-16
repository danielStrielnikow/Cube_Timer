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
#include "mqtt_config.h"
#include "cube_logic.h"
#include "mqtt_client.h"
static const char *TAG = "Cube_Timer";

#define PIN_I2C_SCL GPIO_NUM_22
#define PIN_I2C_SDA GPIO_NUM_21

const uint8_t MPU6050_ADDR = 0x68;
const uint8_t SSD1306_ADDR = 0x3C;

const uint8_t MPU6050_REG_PWR_MGMT_1 = 0x6B;
const uint8_t MPU6050_REG_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU6050_WAKEUP_DATA = 0x00;

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


static cube_timer_t timer;

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

static int isConnected = 0;

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
    isConnected = 1;
}

esp_mqtt_client_handle_t mqttClient;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");;

            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id,
                     (uint8_t)*event->data);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                         strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static void mqtt_start(void) {
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = ADDRESS_URI,
            .address.port = ADDRESS_PORT
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    mqttClient = client;
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


static bool mqtt_publish_task(int side, float battery) {
    char topic[64];
    char json[96];

    snprintf(topic, sizeof(topic), "cube/%s/side", CUBE_ID);
    snprintf(json, sizeof(json), "{\"cubeId\":\"%s\",\"side\":%d,\"battery\":%.2f}", CUBE_ID, side, battery);

    int msg_id = esp_mqtt_client_publish(mqttClient, topic, json, 0, 0, 0);
    if (msg_id == 0) {
        ESP_LOGI(TAG, "Wyslano na temat %s: %s", topic, json);
        return true;
    }

    ESP_LOGW(TAG, "Blad msg_id:%d przy wysylaniu na temat %s", msg_id, topic);
    return false;
}

// ===== bufor na dane, gdy chwilowo nie ma polaczenia z siecia =====
// zeby nie gubic zdarzen jak WiFi na chwile padnie (np. kostka wyjedzie
// poza zasieg albo router sie zrestartuje)

#define OFFLINE_BUFFER_SIZE 20

static int offline_buffer[OFFLINE_BUFFER_SIZE];
static int offline_buffer_count = 0;
static int offline_buffer_head = 0; // indeks najstarszego zapisanego boku

static void offline_buffer_push(int side) {
    if (offline_buffer_count == OFFLINE_BUFFER_SIZE) {
        // bufor pelny - wyrzucamy najstarszy wpis, zeby zrobic miejsce na nowy
        offline_buffer_head = (offline_buffer_head + 1) % OFFLINE_BUFFER_SIZE;
        offline_buffer_count--;
    }
    int tail = (offline_buffer_head + offline_buffer_count) % OFFLINE_BUFFER_SIZE;
    offline_buffer[tail] = side;
    offline_buffer_count++;
}

static bool offline_buffer_peek(int *side) {
    if (offline_buffer_count == 0) {
        return false;
    }
    *side = offline_buffer[offline_buffer_head];
    return true;
}

static void offline_buffer_drop_oldest(void) {
    if (offline_buffer_count == 0) {
        return;
    }
    offline_buffer_head = (offline_buffer_head + 1) % OFFLINE_BUFFER_SIZE;
    offline_buffer_count--;
}

// wysyla bok od razu, jak jest polaczenie. Jak nie ma (albo wysylka sie nie
// udala), zapisuje go do bufora zeby wyslac pozniej.
static void send_or_buffer(int side) {
    if (wifi_connected && mqtt_publish_task(side, 3.85f)) {
        return;
    }
    ESP_LOGW(TAG, "Brak polaczenia - zapisuje bok %d do bufora", side);
    offline_buffer_push(side);
}

// probuje wyslac to, co zalega w buforze, odkad wrocilo polaczenie
static void flush_offline_buffer(void) {
    int side;
    while (wifi_connected && offline_buffer_peek(&side)) {
        if (!mqtt_publish_task(side, 3.85f)) {
            break; // znowu nie wyszlo - sprobujemy w kolejnej petli
        }
        offline_buffer_drop_oldest();
        ESP_LOGI(TAG, "Wyslano zalegly bok %d z bufora", side);
    }
}


void app_main() {
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_connect();

    if (isConnected) mqtt_start();

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config_mcu, &dev_handle_mcu));
    ESP_ERROR_CHECK(ssd1306_init(bus_handle, &dev_cfg, &dev_hdl));

    uint8_t wakeUpMCU[] = {MPU6050_REG_PWR_MGMT_1, MPU6050_WAKEUP_DATA};
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle_mcu, wakeUpMCU, sizeof(wakeUpMCU), -1));

    uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
    uint8_t buffer[6];

    int last_sent_side = -1;
    int64_t last_send_time_us = 0;
    cube_timer_init(&timer);

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

        int64_t now_us = esp_timer_get_time();
        int side = get_cube_side(x, y, z);
        cube_timer_update(&timer, side, now_us);

        if (side != last_sent_side && (now_us - last_send_time_us) >= SEND_COOLDOWN_US) {
            int side_to_send = (side == SLEEP_SIDE) ? 0 : side;
            send_or_buffer(side_to_send);
            last_sent_side = side;
            last_send_time_us = now_us;
        }

        flush_offline_buffer();

        // int wystarczy, bez 64-bitowego formatowania w printf
        int elapsed_ms = (int) (cube_timer_elapsed_us(&timer, now_us) / 1000);

        char lineSide[16];
        char lineTime[32];

        if (side == 0) {
            snprintf(lineSide, sizeof(lineSide), "Bok: ?");
        } else {
            snprintf(lineSide, sizeof(lineSide), "Bok: %d", side);
        }
        snprintf(lineTime, sizeof(lineTime), "Czas: %d.%02ds",
                 elapsed_ms / 1000, (elapsed_ms % 1000) / 10);

        ssd1306_display_text(dev_hdl, 0, lineSide, false);
        ssd1306_display_text(dev_hdl, 1, lineTime, false);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
