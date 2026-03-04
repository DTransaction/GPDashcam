#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "global.h"


esp_err_t init_camera(void) {
	camera_config_t camera_config = {
		.pin_reset		= CONFIG_CAM_PIN_RESET,
		.pin_xclk		= CONFIG_CAM_PIN_XCLK,
		.pin_sccb_sda	= CONFIG_CAM_PIN_SIOD,
		.pin_sccb_scl	= CONFIG_CAM_PIN_SIOC,
		.pin_d7		  = CONFIG_CAM_PIN_D7,
		.pin_d6		  = CONFIG_CAM_PIN_D6,
		.pin_d5		  = CONFIG_CAM_PIN_D5,
		.pin_d4		  = CONFIG_CAM_PIN_D4,
		.pin_d3		  = CONFIG_CAM_PIN_D3,
		.pin_d2		  = CONFIG_CAM_PIN_D2,
		.pin_d1		  = CONFIG_CAM_PIN_D1,
		.pin_d0		  = CONFIG_CAM_PIN_D0,
		.pin_vsync		= CONFIG_CAM_PIN_VSYNC,
		.pin_href		= CONFIG_CAM_PIN_HREF,
		.pin_pclk		= CONFIG_CAM_PIN_PCLK,
		.xclk_freq_hz	= CONFIG_XCLK_FREQ_MHZ * 1000000,
		.ledc_timer	  = LEDC_TIMER_0,
		.ledc_channel	= LEDC_CHANNEL_0,
		.pixel_format	= PIXFORMAT_JPEG,
		.frame_size	  = FRAMESIZE_QHD,
		.jpeg_quality	= CONFIG_JPEG_QUALITY,
		.fb_count		= CONFIG_FB_COUNT
	};

	esp_err_t err = esp_camera_init(&camera_config);
	if (err != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
	} else {
		ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
	}
	return err;
}
void camera_task(void *args) { 
	if (!camera_to_sd_queue) ESP_LOGE(CAMERA_TAG, "Camera to SD queue creation failed"); 
	camera_fb_t *camera_fb; 

	while(1) { 
		if (ulTaskNotifyTake(pdTRUE, 0)) { // Check for notification
			ESP_LOGI(CAMERA_TAG, "Notified to stop recording"); 
			// Need to notify supervisor
			vTaskSuspend(NULL);
		}
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
	}
}
