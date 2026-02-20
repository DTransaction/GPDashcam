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

void mount_sd() { 
    esp_err_t ret;
	sdmmc_card_t *card;
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
    slot_config.d0  = CONFIG_D0_PIN_NUM;
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

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
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
