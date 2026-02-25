#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"

#define ACCEL_TAG "ACCEL_TASK"
#define I2C_FREQUENCY 100000
#define I2C_ACCEL_ADDR 0x53
#define DELAY_MS 100

/**
 * @brief Struct representing acceleration data in 3 axes (x, y, z).
 */
typedef struct {
    float x;  // Acceleration in the X axis (g) 
    float y;  // Acceleration in the Y axis (g) 
    float z;  // Acceleration in the Z axis (g) 
	float total_magnitude; // Total acceleration magnitude
} accel_data_t;

/**
 * @brief FreeRTOS task for continuously reading accelerometer data over I2C.
 *
 * @param pvParameters Task parameter.
 */
void accelerometer_task(void *pvParameters);

#endif // ACCELEROMETER_H

