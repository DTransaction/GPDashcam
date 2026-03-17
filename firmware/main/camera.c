#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "global.h"
#include "stop_detection_runner.h"

//debug
#define ML_W 96
#define ML_H 96
#define CHANNELS 3
#define TENSOR_SIZE (ML_W * ML_H * 3)
int8_t ml_input[TENSOR_SIZE];

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
		// .xclk_freq_hz	= CONFIG_XCLK_FREQ_MHZ * 1000000,
		// .xclk_freq_hz   = 20000000,
		.xclk_freq_hz   = 10000000,
		.ledc_timer	  = LEDC_TIMER_0,
		.ledc_channel	= LEDC_CHANNEL_0,
		// .pixel_format	= PIXFORMAT_JPEG,
		.pixel_format = PIXFORMAT_RGB565,
		.frame_size = FRAMESIZE_VGA,
		.jpeg_quality	= CONFIG_JPEG_QUALITY,
		.fb_count		= 1,
		.fb_location = CAMERA_FB_IN_PSRAM
		// .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
	};

	esp_err_t err = esp_camera_init(&camera_config);
	if (err != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
	} else {
		ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
		sensor_t * s = esp_camera_sensor_get();
        s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
        s->set_aec_value(s, 1100);   // Lower this value to reduce exposure (0-1200)
		
		tinyml_init();

	}
	// sensor_t *s = esp_camera_sensor_get();
	// s->set_gain_ctrl(s, 0);
	// s->set_exposure_ctrl(s, 0);
	// s->set_whitebal(s, 0);
	// s->set_awb_gain(s, 0);
	// s->set_brightness(s, 0);
	// s->set_contrast(s, 0);
	// s->set_saturation(s, 0);
	// s->set_denoise(s, 0);
	return err;
}
void camera_task(void *args) { 
	if (!camera_to_sd_queue) ESP_LOGE(CAMERA_TAG, "Camera to SD queue creation failed"); 
	camera_fb_t *camera_fb; 
	uint8_t ml_activated = 0; 


    char path_txt[32];
	int frame = 0;

	while(1) { 
		if (ulTaskNotifyTakeIndexed(INDEX_IMPACT, pdTRUE, 0)) { 
			ESP_LOGI(CAMERA_TAG, "Notified of an impact"); 
			xTaskNotifyGiveIndexed(supervisor_handle, INDEX_IMPACT); 
		}
		if (ulTaskNotifyTakeIndexed(INDEX_ML, pdTRUE, 0)) { 
			if (!ml_activated) {
				// Configure camera settings for ML
				ESP_LOGI(CAMERA_TAG, "Enabling ML mode"); 
				ml_activated = 1; 
			} else {
				// Revert camera settings for normal pictures
				ESP_LOGI(CAMERA_TAG, "Disabling ML mode"); 
				ml_activated = 0; 
			}
		}
		// ESP_LOGI(CAMERA_TAG, "Capturing image...");
		camera_fb = esp_camera_fb_get();
		if (!camera_fb) {
			ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
			return;
		}

		run_stop_detection(camera_fb->buf, camera_fb->width, camera_fb->height, ml_input);

		// sd card write for debug image processing
        snprintf(path_txt, sizeof(path_txt), "/sdcard/t%05d.bin", frame);
        FILE *f_txt = fopen(path_txt, "wb");

        if (f_txt == NULL) {
            printf("Failed to open tensor file\n");
            return;
        }

        size_t written = fwrite(ml_input, sizeof(int8_t), TENSOR_SIZE, f_txt);

        if (written != TENSOR_SIZE) {
            printf("Write error: %d bytes written\n", (int)written);
        } else {
            printf("Tensor written successfully\n");
        }

        fclose(f_txt);
		frame++;


		
		if (CONFIG_FB_COUNT == 1) { 
			xQueueOverwrite(camera_to_sd_queue, &camera_fb);
		} else if (CONFIG_FB_COUNT > 1) { 
			xQueueSendToFront(camera_to_sd_queue, &camera_fb, portMAX_DELAY); 
		}
		// Temporary delay to slow down camera capture 
		vTaskDelay(pdMS_TO_TICKS(1000));
		// ESP_LOGI(CAMERA_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
	}
}
