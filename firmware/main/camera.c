#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "camera.h"
#include "global.h"
#include "stop_detection_runner.h"

#define ML_W 96
#define ML_H 96
#define CHANNELS 3
#define TENSOR_SIZE (ML_W * ML_H * 3)
int8_t ml_input[TENSOR_SIZE];

SemaphoreHandle_t frame_mutex = NULL;
camera_fb_t *latest_frame = NULL;

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

static camera_config_t ml_camera_config = {
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

static const camera_config_t stream_camera_config = {
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
	.frame_size = FRAMESIZE_QVGA,
	.jpeg_quality = CONFIG_JPEG_QUALITY,
	.fb_count = 2,
	.fb_location = CAMERA_FB_IN_PSRAM
};


static inline int8_t int8_to_percent(int8_t s8) {
	return (int8_t)((s8 + 128) * 0.3922);
}

esp_err_t init_camera() {
	esp_err_t err = esp_camera_init(&default_camera_config);
	if (err != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
	} else {
		tinyml_init();
		ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
		frame_mutex = xSemaphoreCreateMutex();
	}
	return err;
}
void camera_task(void *args) { 
	camera_fb_t *camera_fb; 
	bool ml_activated = false;
	bool sd_activated = true; 

	while(1) { 
		if (ulTaskNotifyTakeIndexed(INDEX_IMPACT, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Notified of an impact"); 
			xTaskNotifyGiveIndexed(supervisor_handle, INDEX_IMPACT); 
		}
		if (ulTaskNotifyTakeIndexed(INDEX_ML_OFF, pdTRUE, 0)) { 
			// Revert camera settings for normal pictures
			ESP_LOGI(CAMERA_TAG, "Disabling ML mode"); 
			ml_activated = false; 
			sd_activated = true; 

			esp_camera_return_all(); 
			esp_camera_reconfigure(&default_camera_config); 
		} 
		if (ulTaskNotifyTakeIndexed(INDEX_ML_FAST, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Enabling ML fast mode"); 
			ml_activated = true; 
			sd_activated = true; 

			sensor_t * s = esp_camera_sensor_get();
			s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
			s->set_aec_value(s, 1100);   // Lower this value to reduce exposure (0-1200)
			ml_camera_config.frame_size = FRAMESIZE_96X96;
			esp_camera_reconfigure(&ml_camera_config); 
		}
		if (ulTaskNotifyTakeIndexed(INDEX_ML_SLOW, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Enabling ML slow mode"); 
			ml_activated = true; 
			sd_activated = true; 

			sensor_t * s = esp_camera_sensor_get();
			s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
			s->set_aec_value(s, 1100);   // Lower this value to reduce exposure (0-1200)
			ml_camera_config.frame_size = FRAMESIZE_VGA;
			esp_camera_reconfigure(&ml_camera_config); 
		}
		if (ulTaskNotifyTakeIndexed(INDEX_LIVE_STREAM, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Enabling ML slow mode"); 
			ml_activated = false; 
			sd_activated = false; 

			sensor_t * s = esp_camera_sensor_get();
			s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
			s->set_aec_value(s, 1100);   // Lower this value to reduce exposure (0-1200)
			esp_camera_reconfigure(&stream_camera_config); 
		}
		// ESP_LOGI(CAMERA_TAG, "Capturing image...");
		camera_fb = esp_camera_fb_get();
		if (!camera_fb) {
			ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
			return;
		}


		if (ml_activated) { 
			int8_t raw_score = run_stop_detection(camera_fb->buf, camera_fb->width, camera_fb->height);
			int8_t percent_score = int8_to_percent(raw_score); 
			xQueueSend(camera_to_display_queue, &percent_score, 0); 
		} 
		// else {
		// 	// Temporary delay to slow down camera capture 
		// 	vTaskDelay(pdMS_TO_TICKS(1000));
		// }

		if(sd_activated) {
			xQueueSend(camera_to_sd_queue, &camera_fb, portMAX_DELAY); 
		}
		else {
			if (xSemaphoreTake(frame_mutex, portMAX_DELAY)) {

				if (latest_frame) {
					esp_camera_fb_return(latest_frame); // free old frame
				}

				latest_frame = camera_fb;  // store newest frame

				xSemaphoreGive(frame_mutex);
			}
		}

		// ESP_LOGI(CAMERA_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
	}
}
			