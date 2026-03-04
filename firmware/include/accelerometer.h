#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include "driver/i2c_master.h"

#define ACCEL_TAG "ACCEL_TASK"
#define I2C_ACCEL_FREQ (100000)
#define I2C_ACCEL_ADDR (0x53)

typedef struct {
    float x;  // Acceleration in the X axis (g) 
    float y;  // Acceleration in the Y axis (g) 
    float z;  // Acceleration in the Z axis (g) 
	float total_magnitude; // Total acceleration magnitude
} accel_data_t;

void init_accel(i2c_master_dev_handle_t *i2c_accel_handle);
void accelerometer_task(void *args);

#endif // ACCELEROMETER_H

