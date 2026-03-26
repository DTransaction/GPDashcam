/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdbool.h>
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "sd.h"
#include "gps.h"
#include "esp_camera.h"
#include "global.h"

#define LATITUDE_INDEX 3
#define LONGITUDE_INDEX 4

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

size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
    FILE *f = (FILE *)arg;
    if (!f) return 0;

    size_t written = fwrite(data, 1, len, f);
    return (written == len) ? len : 0; 
}

static esp_err_t read_file(const char *file_path) {
	FILE *p_file; 
	char line[MAX_CHAR_SIZE]; 
	coordinate_t rl_cam_coord;

    ESP_LOGI(SD_TAG, "Reading file %s", file_path);
	p_file = fopen(file_path, "r"); 
	if (p_file == NULL) { 
		printf("File %s could not be opened.\n", file_path); 
        return ESP_FAIL;
	}

	// Ignore the first line of CSV which are headers
	fgets(line, sizeof(line), p_file);
	while (fgets(line, sizeof(line), p_file)) { 
		char *csv = line; 
		char *p = line;

		for (int csv_index = 0; csv_index <= 4; ++csv_index) {
			// Seek comma or null terminator
			while (!(*p == ',' || *p == '\0')) {
				++p;
				continue;
			}

			// Replace comma with null termination
			*p = '\0';
			++p;

			switch (csv_index) { 
				case LATITUDE_INDEX: 
					rl_cam_coord.latitude = strtod(csv, NULL);
					break;
				case LONGITUDE_INDEX:
					rl_cam_coord.longitude = strtod(csv, NULL);
					break;
			}
			csv = p;
		}

		if (xQueueSend(sd_to_gps_queue, &rl_cam_coord, pdMS_TO_TICKS(500)) != pdPASS) { 
			ESP_LOGE(SD_TAG, "Failed to send red light camera coordinate to GPS queue");
		}
	}
	fclose(p_file);
    return ESP_OK;
}

void sd_task(void *args) { 
	esp_err_t ret;
	gps_data_t gps_data; 
	camera_fb_t *camera_fb = NULL; 
	uint16_t img_count = 0; 
	bool ml_activated = false;

	char buffer[MAX_CHAR_SIZE];
	const char *GPS_FILE_PATH = MOUNT_POINT"/gps_data.log"; 
	const char *RED_LIGHT_CAMERA_FILE_PATH = MOUNT_POINT"/Red_Light_Camera_Locations.csv"; 
	char file_path[50]; 
	TickType_t time_start = 1;
	TickType_t time_end = 1;
	uint32_t time;

	read_file(RED_LIGHT_CAMERA_FILE_PATH);

	if (!camera_to_sd_queue) ESP_LOGE(SD_TAG, "Camera to SD queue creation failed"); 
	while (1) { 
		if (ulTaskNotifyTakeIndexed(INDEX_ML_OFF, pdTRUE, 0)) ml_activated = false;
		if (ulTaskNotifyTakeIndexed(INDEX_ML_SLOW, pdTRUE, 0)) ml_activated = true;
		if (ulTaskNotifyTakeIndexed(INDEX_ML_FAST, pdTRUE, 0)) ml_activated = true;
		if (xQueueReceive(camera_to_sd_queue, &camera_fb, portMAX_DELAY)) {
			sprintf(file_path, "%s/image%d.jpg", MOUNT_POINT, img_count++);
			FILE *file = fopen(file_path, "wb"); 

			if (camera_fb->format == PIXFORMAT_RGB565) {
				bool ok = frame2jpg_cb(camera_fb, 90, jpg_encode_stream, file);
				if(!ok) {
					ESP_LOGE(SD_TAG, "JPEG compression failed");
				}
			} else {
				fwrite(camera_fb->buf, 1, camera_fb->len, file); 
			}
			fclose(file); 

			time_end = xTaskGetTickCount(); 
			time = (time_end - time_start) * portTICK_PERIOD_MS;
			ESP_LOGI(SD_TAG, "%s, %zu bytes, %lums, %lu FPS", file_path, camera_fb->len, time, 1000/time); 
			time_start = xTaskGetTickCount(); 
			esp_camera_fb_return(camera_fb);

			// if (!ml_activated) {
			// 	esp_camera_fb_return(camera_fb);
			// }
			ESP_LOGI(SD_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
		}
	}
}
