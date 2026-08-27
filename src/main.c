#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#define PIN_I2C_SCL_GPIO_NUM_22 GPIO_NUM_22
#define PIN_I2C_SDA_GPIO_NUM_21 GPIO_NUM_21
const uint8_t MCU_6050_ADDR = 0x68;
const uint8_t MCU_6050_WAKEUP = 0x6B;
const uint8_t MCU_6050_WAKEUP_DATA = 0x00;

i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

i2c_device_config_t dev_config = {
    .device_address = MCU_6050_ADDR,
    .scl_speed_hz = 400000,
};

void app_main() {
    i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);

    uint8_t wakeUpMCU[] = {MCU_6050_WAKEUP, MCU_6050_WAKEUP_DATA};

    i2c_master_transmit(dev_handle, wakeUpMCU, sizeof(wakeUpMCU), -1);


}







