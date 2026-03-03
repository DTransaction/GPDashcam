#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "queues.h"

void camera_task(void *args) { 
	if (!camera_to_sd_queue) ESP_LOGE(CAMERA_TAG, "Camera to SD queue creation failed"); 
	camera_fb_t *camera_fb; 

    ESP_LOGI(CAMERA_TAG, "Capturing image...");
    camera_fb = esp_camera_fb_get();
    if (!camera_fb) {
        ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
        return;
    }
	ESP_LOGI(CAMERA_TAG, "Picture captured! Size: %zu bytes", camera_fb->len);
	
	if (CONFIG_FB_COUNT == 1) { 
		xQueueOverwrite(camera_to_sd_queue, &camera_fb);
	} else if (CONFIG_FB_COUNT > 1) { 
		xQueueSendToFront(camera_to_sd_queue, &camera_fb, portMAX_DELAY); 
	}
	vTaskSuspend(NULL);
}
