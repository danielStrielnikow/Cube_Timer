#include <stdio.h>


#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000
#define I2C_TIMEOUT_MS      1000


#define MCU6050_REGISTRY    0x68
#define MCU6050_POWER       0x6B
#define MCU6050_ACCEL       0x3B



void setup()
{


}