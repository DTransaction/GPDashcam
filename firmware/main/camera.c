#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera.h"
#include "esp_timer.h"

#include "person_detection_runner.h"

#define VIDEO_FILE_PATH "/sdcard/video.mj2"
    #define RECORD_TIME_MS  45000   // record 10 seconds

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

// -- TINYML --
#define ML_W 96
#define ML_H 96
#define CHANNELS 3
#define TENSOR_SIZE (ML_W * ML_H * 3)
int8_t ml_input[TENSOR_SIZE];

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
    // .pixel_format    = PIXFORMAT_JPEG,  
    // .pixel_format = PIXFORMAT_GRAYSCALE,
    .pixel_format = PIXFORMAT_RGB565,
    // .frame_size      = FRAMESIZE_FHD,
    .frame_size      = FRAMESIZE_VGA,
    // .frame_size      = FRAMESIZE_QVGA, //works
    // .frame_size      = FRAMESIZE_CIF, //works less well?
    // .frame_size = FRAMESIZE_96X96,
    // .frame_size = FRAMESIZE_320X320,
    .jpeg_quality    = 10,
    .fb_count        = 1,
    .fb_location = CAMERA_FB_IN_PSRAM
};


static esp_err_t init_camera(void) {
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
    return err;
}


size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    FILE *f = (FILE *)arg;
    if (!f) return 0;

    size_t written = fwrite(data, 1, len, f);
    return (written == len) ? len : 0;
}

void camera_task(void *args) {
    ESP_LOGI(CAMERA_TAG, "Starting camera");

    if (init_camera() != ESP_OK) {
        ESP_LOGE(CAMERA_TAG, "Camera init failed");
        vTaskDelete(NULL);
    }

    ESP_LOGI(CAMERA_TAG, "Recording images...");

    int64_t start = esp_timer_get_time(); // µs

    int frame = 0;
    char path[32];
    char path_txt[32];

    while ((esp_timer_get_time() - start) < (RECORD_TIME_MS * 1000)) {

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(CAMERA_TAG, "Capture failed");
            continue;
        }

        run_stop_detection(fb->buf, fb->width, fb->height, ml_input);
        
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
        
        //convert frame to jpg to save to sd card
        sprintf(path, "/sdcard/f%05d.jpg", frame);
        FILE *f = fopen(path, "wb");
        bool ok = frame2jpg_cb(fb, 90, jpg_encode_stream, f);
        fclose(f);

        if(!ok) {
            ESP_LOGE(CAMERA_TAG, "JPEG compression failed");
        }

        ESP_LOGI(CAMERA_TAG, "Frame: %zu bytes", fb->len);

        esp_camera_fb_return(fb);

        ++frame;
    }

    ESP_LOGI(CAMERA_TAG, "Images saved to SD card");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
