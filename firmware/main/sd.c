/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd.h"
#include "gps.h"
#include "esp_camera.h"

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

static esp_err_t read_file(const char *path) {
    ESP_LOGI(SD_TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(SD_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(SD_TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

void sd_task(void *args) { 
    esp_err_t ret;
	QueueHandle_t **queues = (QueueHandle_t **)args; 
	QueueHandle_t *gps_to_sd_queue = queues[0]; 
	QueueHandle_t *camera_to_sd_queue = queues[1]; 

	gps_data_t *gps_data = malloc(sizeof(gps_data_t)); 
	camera_fb_t *camera_fb = malloc(sizeof(camera_fb_t)); 

	char data[MAX_CHAR_SIZE];
	const char *GPS_FILE_PATH = MOUNT_POINT"/gps_data.log"; 
	const char *CAMERA_FILE_PATH = MOUNT_POINT"/image.jpg"; 

	while (1) { 
		// GPS data logging 
		if (xQueueReceive(*gps_to_sd_queue, gps_data, 0)) {
			snprintf(data, MAX_CHAR_SIZE, "%02d-%02d-%04d %02d:%02d:%02d %f, %f, %fm/s, %.02f degrees, heading %s\n", 
						gps_data->day, 
						gps_data->month, 
						gps_data->year, 
						gps_data->hour, 
						gps_data->minute, 
						gps_data->second, 
						gps_data->latitude,
						gps_data->longitude,
						gps_data->speed,
						gps_data->cog,
						gps_data->direction
						); 
			ret = append_file(GPS_FILE_PATH, data); 
			if (ret != ESP_OK) return;

			// Open file for reading
			ret = read_file(GPS_FILE_PATH);
			if (ret != ESP_OK) return;
		}

		if (xQueueReceive(*camera_to_sd_queue, camera_fb, 1000)) {
			FILE *file = fopen(CAMERA_FILE_PATH, "w"); 
			fwrite(camera_fb->buf, 1, camera_fb->len, file); 
			fclose(file); 
			ESP_LOGI(SD_TAG, "Image saved to %s", CAMERA_FILE_PATH); 
		}
		ESP_LOGI(SD_TAG, "Stack high water mark: %d", uxTaskGetStackHighWaterMark(NULL)); 
	}
	// Unmount partition and disable SDMMC peripheral
	// esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
	// ESP_LOGI(SD_TAG, "Card unmounted");
}
