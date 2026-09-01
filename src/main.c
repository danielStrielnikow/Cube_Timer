#include <esp_log.h>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "ssd1306.h"
#include <assert.h>

static const char *TAG = "Cube_Timer";

#define PIN_I2C_SCL_GPIO_NUM_22 GPIO_NUM_22
#define PIN_I2C_SDA_GPIO_NUM_21 GPIO_NUM_21
const uint8_t MCU_6050_ADDR = 0x68;
const uint8_t MCU_6050_WAKEUP = 0x6B;
const uint8_t MCU_6050_WAKEUP_DATA = 0x00;
const uint8_t SSD1306_ADDR = 0x3C;

static i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle_mcu;
static ssd1306_config_t dev_cfg = SSD1306_128x32_CONFIG_DEFAULT;
static ssd1306_handle_t dev_hdl;

i2c_device_config_t dev_config_mcu = {
    .device_address = MCU_6050_ADDR,
    .scl_speed_hz = 400000,
};


void app_main() {
    i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    i2c_master_bus_add_device(bus_handle, &dev_config_mcu, &dev_handle_mcu);
    ssd1306_init(bus_handle, &dev_cfg, &dev_hdl);
    if (dev_hdl == NULL) {
        ESP_LOGE(TAG, "ssd1306 handle init failed");
        assert(dev_hdl);
    }


    uint8_t wakeUpMCU[] = {MCU_6050_WAKEUP, MCU_6050_WAKEUP_DATA};

    i2c_master_transmit(dev_handle_mcu, wakeUpMCU, sizeof(wakeUpMCU), -1);


    uint8_t buf[1] = {0x3B};
    uint8_t buffer[6];


    ESP_LOGI(TAG, "######################## SSD1306 - START #########################");
    ssd1306_clear_display(dev_hdl, false);
    while (1) {
        i2c_master_transmit_receive(dev_handle_mcu, buf, sizeof(buf), buffer, sizeof(buffer), -1);
        int16_t x = (buffer[0] << 8) | buffer[1];
        int16_t y = (buffer[2] << 8) | buffer[3];
        int16_t z = (buffer[4] << 8) | buffer[5];
        char lineX[16];
        char lineY[16];
        char lineZ[16];

        snprintf(lineX, sizeof(lineX), "X:%-6d", x);
        snprintf(lineY, sizeof(lineY), "Y:%-6d", y);
        snprintf(lineZ, sizeof(lineZ), "Z:%-6d", z);

        ssd1306_display_text(dev_hdl, 0, lineX, false);
        ssd1306_display_text(dev_hdl, 1, lineY, false);
        ssd1306_display_text(dev_hdl, 2, lineZ, false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
