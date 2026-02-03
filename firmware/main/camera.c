#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "esp_timer.h"

#define VIDEO_FILE_PATH "/sdcard/video.mj2"
#define RECORD_TIME_MS  6000   // record 5 seconds
#define FRAME_DELAY_MS  0    // ~10 FPS

// ESP32S3 (WROOM) OV5640 pin mapping
#define CAM_PIN_RESET   -1   // Software reset
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5
#define CAM_PIN_D0      11
#define CAM_PIN_D1      9
#define CAM_PIN_D2      8
#define CAM_PIN_D3      10
#define CAM_PIN_D4      12
#define CAM_PIN_D5      18
#define CAM_PIN_D6      17
#define CAM_PIN_D7      16
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

static camera_config_t camera_config = {
    .pin_reset       = CAM_PIN_RESET,
    .pin_xclk        = CAM_PIN_XCLK,
    .pin_sccb_sda    = CAM_PIN_SIOD,
    .pin_sccb_scl    = CAM_PIN_SIOC,
    .pin_d7          = CAM_PIN_D7,
    .pin_d6          = CAM_PIN_D6,
    .pin_d5          = CAM_PIN_D5,
    .pin_d4          = CAM_PIN_D4,
    .pin_d3          = CAM_PIN_D3,
    .pin_d2          = CAM_PIN_D2,
    .pin_d1          = CAM_PIN_D1,
    .pin_d0          = CAM_PIN_D0,
    .pin_vsync       = CAM_PIN_VSYNC,
    .pin_href        = CAM_PIN_HREF,
    .pin_pclk        = CAM_PIN_PCLK,
    .xclk_freq_hz    = 20000000,
    .ledc_timer      = LEDC_TIMER_0,
    .ledc_channel    = LEDC_CHANNEL_0,
    .pixel_format    = PIXFORMAT_JPEG,
    .frame_size      = FRAMESIZE_FHD,
    // .frame_size      = FRAMESIZE_VGA,
    .jpeg_quality    = 10,
    .fb_count        = 2,
    .fb_location = CAMERA_FB_IN_PSRAM
};

static esp_err_t init_camera(void)
{
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
        sensor_t * s = esp_camera_sensor_get();
        s->set_exposure_ctrl(s, 0); // 0 = Disable Auto Exposure, 1 = Enable
        s->set_aec_value(s, 1500);   // Lower this value to reduce exposure (0-1200)
    }
    return err;
}

// void camera_task(void *args) { 
// 	QueueHandle_t **queues = (QueueHandle_t **)args; 
// 	QueueHandle_t *camera_to_sd_queue = queues[0]; 

// 	camera_fb_t *camera_fb = malloc(sizeof(camera_fb_t)); 

//     ESP_LOGI(CAMERA_TAG, "Starting camera test");

//     if (init_camera() != ESP_OK) {
//         ESP_LOGE(CAMERA_TAG, "Failed to initialize camera");
//         return;
//     }

//     ESP_LOGI(CAMERA_TAG, "Capturing image...");
//     camera_fb = esp_camera_fb_get();
//     if (!camera_fb) {
//         ESP_LOGE(CAMERA_TAG, "Failed to get frame buffer");
//         return;
//     }
	
// 	xQueueOverwrite(*camera_to_sd_queue, camera_fb);

//     ESP_LOGI(CAMERA_TAG, "Picture captured! Size: %zu bytes", camera_fb->len);

//     esp_camera_fb_return(camera_fb);
// 	ESP_LOGI(CAMERA_TAG, "Returned FB"); 

// 	while (1) vTaskDelay(pdMS_TO_TICKS(100000));
// }


void camera_task(void *args)
{
    vTaskDelay(pdMS_TO_TICKS(5000));

    // FILE *video = fopen(VIDEO_FILE_PATH, "wb");
    // if (!video) {
    //     ESP_LOGE(CAMERA_TAG, "Failed to open video file");
    //     vTaskDelete(NULL);
    // }

    ESP_LOGI(CAMERA_TAG, "Starting camera");

    if (init_camera() != ESP_OK) {
        ESP_LOGE(CAMERA_TAG, "Camera init failed");
        vTaskDelete(NULL);
    }

    ESP_LOGI(CAMERA_TAG, "Recording images...");

    int64_t start = esp_timer_get_time(); // µs

    int frame = 0;
    char path[32];
    while ((esp_timer_get_time() - start) < (RECORD_TIME_MS * 1000)) {

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(CAMERA_TAG, "Capture failed");
            continue;
        }

        // fwrite(fb->buf, 1, fb->len, video);
        // fflush(video);

        //take multiple pictures
        sprintf(path, "/sdcard/f%05d.jpg", frame++);
        FILE *f = fopen(path, "wb");
        fwrite(fb->buf, 1, fb->len, f);
        fclose(f);


        ESP_LOGI(CAMERA_TAG, "Frame: %zu bytes", fb->len);

        esp_camera_fb_return(fb);

        // vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }

    // fclose(video);

    ESP_LOGI(CAMERA_TAG, "Images saved to SD card");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
