#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "global.h"
#include "stop_detection_runner.h"

#define ML_W 96
#define ML_H 96
#define CHANNELS 3
#define TENSOR_SIZE (ML_W * ML_H * 3)
int8_t ml_input[TENSOR_SIZE];

static const camera_config_t default_camera_config = {
	.pin_reset = CONFIG_CAM_PIN_RESET,
	.pin_xclk = CONFIG_CAM_PIN_XCLK,
	.pin_sccb_sda = CONFIG_CAM_PIN_SIOD,
	.pin_sccb_scl = CONFIG_CAM_PIN_SIOC,
	.pin_d7 = CONFIG_CAM_PIN_D7,
	.pin_d6 = CONFIG_CAM_PIN_D6,
	.pin_d5 = CONFIG_CAM_PIN_D5,
	.pin_d4 = CONFIG_CAM_PIN_D4,
	.pin_d3 = CONFIG_CAM_PIN_D3,
	.pin_d2 = CONFIG_CAM_PIN_D2,
	.pin_d1 = CONFIG_CAM_PIN_D1,
	.pin_d0 = CONFIG_CAM_PIN_D0,
	.pin_vsync = CONFIG_CAM_PIN_VSYNC,
	.pin_href = CONFIG_CAM_PIN_HREF,
	.pin_pclk = CONFIG_CAM_PIN_PCLK,
	.xclk_freq_hz = CONFIG_XCLK_FREQ_MHZ * 1000000,
	.ledc_timer = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,
	.pixel_format = PIXFORMAT_JPEG,
	.frame_size = FRAMESIZE_HD,
	.jpeg_quality = CONFIG_JPEG_QUALITY,
	.fb_count = 3,
	.fb_location = CAMERA_FB_IN_PSRAM
};

static const camera_config_t ml_camera_config = {
	.pin_reset = CONFIG_CAM_PIN_RESET,
	.pin_xclk = CONFIG_CAM_PIN_XCLK,
	.pin_sccb_sda = CONFIG_CAM_PIN_SIOD,
	.pin_sccb_scl = CONFIG_CAM_PIN_SIOC,
	.pin_d7 = CONFIG_CAM_PIN_D7,
	.pin_d6 = CONFIG_CAM_PIN_D6,
	.pin_d5 = CONFIG_CAM_PIN_D5,
	.pin_d4 = CONFIG_CAM_PIN_D4,
	.pin_d3 = CONFIG_CAM_PIN_D3,
	.pin_d2 = CONFIG_CAM_PIN_D2,
	.pin_d1 = CONFIG_CAM_PIN_D1,
	.pin_d0 = CONFIG_CAM_PIN_D0,
	.pin_vsync = CONFIG_CAM_PIN_VSYNC,
	.pin_href = CONFIG_CAM_PIN_HREF,
	.pin_pclk = CONFIG_CAM_PIN_PCLK,
	.xclk_freq_hz = 20000000,
	.ledc_timer = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,
	.pixel_format = PIXFORMAT_RGB565,
	.frame_size = FRAMESIZE_96X96,
	.jpeg_quality = 10,
	.fb_count = 1,
	.fb_location = CAMERA_FB_IN_PSRAM
};

esp_err_t init_camera() {
	esp_err_t err = esp_camera_init(&default_camera_config);
	if (err != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
	} else {
		tinyml_init();
		ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
	}
	return err;
}
void camera_task(void *args) { 
	camera_fb_t *camera_fb; 
	bool ml_activated = false; 

	while(1) { 
		if (ulTaskNotifyTakeIndexed(INDEX_IMPACT, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Notified of an impact"); 
			xTaskNotifyGiveIndexed(supervisor_handle, INDEX_IMPACT); 
		}
		if (ulTaskNotifyTakeIndexed(INDEX_ML, pdTRUE, 0)) { 
			esp_camera_return_all(); 
			if (!ml_activated) {
				// Configure camera settings for ML
				ESP_LOGI(CAMERA_TAG, "Enabling ML mode"); 
				ml_activated = true; 
				sensor_t * s = esp_camera_sensor_get();
				s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
				s->set_aec_value(s, 1100);   // Lower this value to reduce exposure (0-1200)
				esp_camera_reconfigure(&ml_camera_config); 
			} else {
				// Revert camera settings for normal pictures
				ESP_LOGI(CAMERA_TAG, "Disabling ML mode"); 
				ml_activated = false; 
				esp_camera_reconfigure(&default_camera_config); 
			}
		}
		// ESP_LOGI(CAMERA_TAG, "Capturing image...");
		camera_fb = esp_camera_fb_get();
		if (!camera_fb) {
			ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
			return;
		}

		if (ml_activated) { 
			run_stop_detection(camera_fb->buf, camera_fb->width, camera_fb->height);
		} else {
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		xQueueSendToFront(camera_to_sd_queue, &camera_fb, portMAX_DELAY); 
		// Temporary delay to slow down camera capture 
		// ESP_LOGI(CAMERA_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
	}
}
