/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "sd.h"
#include "gps.h"
#include "esp_camera.h"
#include "global.h"

void mount_sd(sdmmc_card_t* card) { 
	esp_err_t ret;
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz

	// Options for mounting the filesystem.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	ESP_LOGI(SD_TAG, "Initializing SD card");
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
	slot_config.width = 4;
	slot_config.clk = CONFIG_CLK_PIN_NUM;
	slot_config.cmd = CONFIG_CMD_PIN_NUM;
	slot_config.d0 = CONFIG_D0_PIN_NUM;
	slot_config.d1 = CONFIG_D1_PIN_NUM;
	slot_config.d2 = CONFIG_D2_PIN_NUM;
	slot_config.d3 = CONFIG_D3_PIN_NUM;

	ESP_LOGI(SD_TAG, "Mounting filesystem");
	ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(SD_TAG, "Failed to mount filesystem. ");
		} else {
			ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). "
					 "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
		}
		return;
	}
	ESP_LOGI(SD_TAG, "Filesystem mounted");
}
static esp_err_t append_file(const char *path, char *data) {
    ESP_LOGI(SD_TAG, "Opening file %s", path);
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        ESP_LOGE(SD_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(SD_TAG, "File written");

    return ESP_OK;
}

void sd_task(void *args) { 
	esp_err_t ret;
	gps_data_t gps_data; 
	camera_fb_t *camera_fb = NULL; 
	uint16_t img_count = 0; 

	char buffer[MAX_CHAR_SIZE];
	const char *GPS_FILE_PATH = MOUNT_POINT"/gps_data.log"; 
	char file_path[50]; 
	TickType_t time_start = 1;
	TickType_t time_end = 1;
	uint32_t time;

	if (!camera_to_sd_queue) ESP_LOGE(SD_TAG, "Camera to SD queue creation failed"); 
	while (1) { 
		if (xQueueReceive(camera_to_sd_queue, &camera_fb, portMAX_DELAY)) {
			snprintf(file_path, 30, "%s/image%d.jpg", MOUNT_POINT, img_count++);
			FILE *file = fopen(file_path, "wb"); 
			fwrite(camera_fb->buf, 1, camera_fb->len, file); 
			fclose(file); 

			time_end = xTaskGetTickCount(); 
			time = (time_end - time_start) * portTICK_PERIOD_MS;
			ESP_LOGI(SD_TAG, "%s, %zu bytes, %lums, %lu FPS", file_path, camera_fb->len, time, 1000/time); 
			time_start = xTaskGetTickCount(); 
			esp_camera_fb_return(camera_fb);
			// temporary to stop camera after 60 pictures
			if (img_count >= 60) vTaskSuspend(camera_handle); 
			// ESP_LOGI(SD_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
		}
		// GPS data logging 
		if (xQueueReceive(gps_to_sd_queue, &gps_data, 0)) {
			snprintf(buffer, MAX_CHAR_SIZE, "%02d-%02d-%04d %02d:%02d:%02d %f, %f, %fm/s, %.02f degrees, heading %s\n", 
						gps_data.day, 
						gps_data.month, 
						gps_data.year, 
						gps_data.hour, 
						gps_data.minute, 
						gps_data.second, 
						gps_data.latitude,
						gps_data.longitude,
						gps_data.speed,
						gps_data.cog,
						gps_data.direction
						); 
			ret = append_file(GPS_FILE_PATH, buffer); 
			if (ret != ESP_OK) return;
		}
	}
}
