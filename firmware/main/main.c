#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "softap.h"

#include "accelerometer.h"
#include "camera.h"
#include "display.h"
#include "file_server.h"
#include "global.h"
#include "gpio.h"
#include "gps.h"
#include "i2c.h"
#include "sd.h"
#include "uart.h"

#define MAIN_TAG "MAIN_TASK"
#define SUPERVISOR_TAG "SUPERVISOR_TASK"

TaskHandle_t supervisor_handle; 
TaskHandle_t accel_handle; 
TaskHandle_t gps_handle; 
TaskHandle_t display_handle; 
TaskHandle_t sd_handle; 
TaskHandle_t camera_handle; 
QueueHandle_t accel_to_display_queue; 
QueueHandle_t gps_to_display_queue; 
QueueHandle_t gps_to_sd_queue; 
QueueHandle_t sd_to_gps_queue; 
QueueHandle_t camera_to_sd_queue; 
QueueHandle_t gpio_event_queue; 
i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t i2c_accel_handle;
esp_lcd_panel_handle_t *panel;

void supervisor_task(void *args) { 
	while (1) { 
		// ESP_LOGI("SUPERVISOR_TASK", "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
		ulTaskNotifyTakeIndexed(INDEX_IMPACT, pdTRUE, portMAX_DELAY); // Block waiting for accelerometer to notify
		ESP_LOGI(SUPERVISOR_TAG, "Suspending accel, GPS, and display"); 
		vTaskSuspend(accel_handle); 
		vTaskSuspend(gps_handle); 
		vTaskSuspend(display_handle); 
		ESP_LOGI(SUPERVISOR_TAG, "Notifying camera"); 
		xTaskNotifyGiveIndexed(camera_handle, INDEX_IMPACT); 
		ulTaskNotifyTakeIndexed(INDEX_IMPACT, pdTRUE, portMAX_DELAY); 
		ESP_LOGI(SUPERVISOR_TAG, "Received ACK from camera"); 
		ESP_LOGI(SUPERVISOR_TAG, "Resuming accel, GPS, and display"); 
		vTaskResume(accel_handle); 
		vTaskResume(gps_handle); 
		vTaskResume(display_handle); 
	}
}

void app_main(void) {
	panel = malloc(sizeof(esp_lcd_panel_handle_t));
	sdmmc_card_t card;

	if (init_camera() != ESP_OK) {
		ESP_LOGE(MAIN_TAG, "Failed to initialize camera");
		return;
	}

	mount_sd(&card); 
	init_i2c(&i2c_bus); 
	init_uart(); 
	init_display(panel); 
	init_accel(&i2c_accel_handle);
	init_gpio(); 

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_softap();
	ESP_ERROR_CHECK(example_start_file_server(MOUNT_POINT));

	accel_to_display_queue = xQueueCreate(1, sizeof(accel_data_t)); 
	gps_to_display_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	gps_to_sd_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	sd_to_gps_queue = xQueueCreate(10, sizeof(coordinate_t)); 
	camera_to_sd_queue = xQueueCreate(CONFIG_FB_COUNT, sizeof(camera_fb_t*)); 
	gpio_event_queue = xQueueCreate(5, sizeof(uint32_t));

	if (!accel_to_display_queue) ESP_LOGE(MAIN_TAG, "Accel to display queue creation failed"); 
	if (!gps_to_display_queue) ESP_LOGE(MAIN_TAG, "GPS to display queue creation failed"); 
	if (!gps_to_sd_queue) ESP_LOGE(MAIN_TAG, "GPS to SD queue creation failed"); 
	if (!sd_to_gps_queue) ESP_LOGE(MAIN_TAG, "SD to GPS queue creation failed"); 
	if (!camera_to_sd_queue) ESP_LOGE(MAIN_TAG, "Camera to SD queue creation failed"); 
	if (!gpio_event_queue) ESP_LOGE(MAIN_TAG, "GPIO event queue creation failed"); 

	xTaskCreatePinnedToCore(supervisor_task, SUPERVISOR_TAG, 4096, NULL, 6, &supervisor_handle, 1);
	ESP_LOGI(MAIN_TAG, "Supervisor task created");
	xTaskCreatePinnedToCore(display_task, DISPLAY_TAG, 4096, NULL, 3, &display_handle, 1);
	ESP_LOGI(MAIN_TAG, "Display task created");
	xTaskCreatePinnedToCore(accelerometer_task, ACCEL_TAG, 4500, NULL, 3, &accel_handle, 1); 
	ESP_LOGI(MAIN_TAG, "Accelerometer task created");
	xTaskCreatePinnedToCore(gps_task, GPS_TAG, 6500, NULL, 3, &gps_handle, 1);
	ESP_LOGI(MAIN_TAG, "GPS task created");
	xTaskCreatePinnedToCore(camera_task, CAMERA_TAG, 8000, NULL, 5, &camera_handle, 0); 
	ESP_LOGI(MAIN_TAG, "Camera task created");
	xTaskCreatePinnedToCore(sd_task, SD_TAG, 8000, NULL, 6, &sd_handle, 0);
	ESP_LOGI(MAIN_TAG, "SD task created");

	vTaskSuspend(NULL);
}

