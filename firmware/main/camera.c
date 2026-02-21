#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"

void camera_task(void *args) { 
	QueueHandle_t **queues = (QueueHandle_t **)args; 
	QueueHandle_t *camera_to_sd_queue = queues[0]; 

	camera_fb_t *camera_fb = malloc(sizeof(camera_fb_t)); 

    ESP_LOGI(CAMERA_TAG, "Starting camera test");

    ESP_LOGI(CAMERA_TAG, "Capturing image...");
    camera_fb = esp_camera_fb_get();
    if (!camera_fb) {
        ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
        return;
    }
	
	xQueueOverwrite(*camera_to_sd_queue, camera_fb);

    ESP_LOGI(CAMERA_TAG, "Picture captured! Size: %zu bytes", camera_fb->len);

    esp_camera_fb_return(camera_fb);
	ESP_LOGI(CAMERA_TAG, "Returned FB"); 

	while (1) vTaskDelay(pdMS_TO_TICKS(100000));
}
