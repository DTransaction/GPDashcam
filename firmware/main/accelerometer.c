#include <stdint.h>
#include "driver/i2c_master.h"
#include "accelerometer.h"
#include "esp_log.h"
#include <math.h>
#include "queues.h"

#define I2C_TOOL_TIMEOUT_VALUE_MS (50)

static uint8_t i2cget(i2c_master_dev_handle_t i2c_dev_handle, uint8_t data_addr, uint8_t *data) {
    uint8_t len = 1;

    esp_err_t ret = i2c_master_transmit_receive(i2c_dev_handle, (uint8_t*)&data_addr, 1, data, len, I2C_TOOL_TIMEOUT_VALUE_MS);
    if (ret == ESP_OK) {
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(ACCEL_TAG, "Bus is busy");
		return 1; 
    } else {
        ESP_LOGW(ACCEL_TAG, "Read failed");
		return 1; 
    }
    return 0;
}

static uint8_t i2cset(i2c_master_dev_handle_t i2c_dev_handle, uint8_t data_addr, uint8_t data) {
    int len = 1;

    uint8_t *register_data = malloc(len + 1);
    register_data[0] = data_addr;
	register_data[1] = data;
    esp_err_t ret = i2c_master_transmit(i2c_dev_handle, register_data, len + 1, I2C_TOOL_TIMEOUT_VALUE_MS);
    if (ret == ESP_OK) {
		ESP_LOGI(ACCEL_TAG, "Write OK");
	} else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(ACCEL_TAG, "Bus is busy");
		return 1; 
    } else {
        ESP_LOGW(ACCEL_TAG, "Write Failed");
		return 1; 
    }

    free(register_data);
    return 0;
}

void accelerometer_task(void *args) { 
	extern TaskHandle_t supervisor_handle;
	extern i2c_master_dev_handle_t i2c_accel_handle;
	if (!accel_to_display_queue) ESP_LOGE(ACCEL_TAG, "Accel to display queue creation failed"); 

	accel_data_t accel_data; 
	uint8_t data_byte; 

	int16_t raw_x; 
	int16_t raw_y; 
	int16_t raw_z; 
	float x; 
	float y; 
	float z; 

	// TODO: Figure out why I2C device needs to be woken up. 
	// Current inelegant solution is to poke it with a read. 
	i2cget(i2c_accel_handle, 0x32, &data_byte);

	// Configure accelerometer 
	i2cset(i2c_accel_handle, 0x2d, 0x00);
	i2cset(i2c_accel_handle, 0x2d, 0x08);
	i2cset(i2c_accel_handle, 0x31, 0x01);

	// Continuously retreive accel data 
	while (1) { 
		// Collect LSB then MSB of x value
		i2cget(i2c_accel_handle, 0x32, &data_byte);
		raw_x = data_byte; 
		i2cget(i2c_accel_handle, 0x33, &data_byte);
		raw_x = raw_x | (data_byte << 8); 
		// Collect LSB then MSB of y value
		i2cget(i2c_accel_handle, 0x34, &data_byte);
		raw_y = data_byte; 
		i2cget(i2c_accel_handle, 0x35, &data_byte);
		raw_y = raw_y | (data_byte << 8); 
		// Collect LSB then MSB of z value
		i2cget(i2c_accel_handle, 0x36, &data_byte);
		raw_z = data_byte; 
		i2cget(i2c_accel_handle, 0x37, &data_byte);
		raw_z = raw_z | (data_byte << 8); 
		
		x = (float)raw_x/128;
		y = (float)raw_y/128;
		z = (float)raw_z/128;
		accel_data.total_magnitude = sqrtf(x*x + y*y + z*z);
		accel_data.x = x;
		accel_data.y = y;
		accel_data.z = z;

		/*ESP_LOGI(ACCEL_TAG, "x=%.2f, y=%.2f, z=%.2f", (float)raw_x/128, (float)raw_y/128, (float)raw_z/128); */
		// ESP_LOGI("ACCEL", "Queue handle: %p", accel_to_display_queue);
		// ESP_LOGI(ACCEL_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
		if (accel_data.total_magnitude >= CONFIG_IMPACT_GFORCE_THRESHOLD) { 
			ESP_LOGI(ACCEL_TAG, "IMPACT DETECTED"); 
			if (supervisor_handle != NULL) { 
				xTaskNotifyGive(supervisor_handle); 
			}
		} else { 
			xQueueOverwrite(accel_to_display_queue, &accel_data);
		}
		vTaskDelay(pdMS_TO_TICKS(1000/CONFIG_POLL_RATE));
	}

	// If task exits somehow, clean up 
	ESP_LOGE(ACCEL_TAG, "Unexpectedly exited");
	vTaskDelete(NULL); 
}

