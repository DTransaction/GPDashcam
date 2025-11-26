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
#include "sd_test_io.h"
#include "sd.h"
#include "esp_timer.h"
#include "esp_random.h"

void sd_task(void *args) { 
	const char *VIDEO_FILE_PATH = MOUNT_POINT"/4MB_file.bin";

    esp_err_t ret;
	sdmmc_card_t *card;
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	host.max_freq_khz = 40000000; // 20 MHz

    // Options for mounting the filesystem.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };


	ESP_LOGI(SD_TAG, "Initializing SD card");
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if IS_UHS1
    slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif

    /*slot_config.width = 1;*/
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

	float test_result; 
	float test_average = 0; 
	char *item = (char *)malloc(1024*16); // 16KB
	char *item2 = (char *)malloc(1024*16); // 16KB
	if (item == NULL) { 
		ESP_LOGI(SD_TAG, "Malloc failed"); 
	}
	if (item2 == NULL) { 
		ESP_LOGI(SD_TAG, "Malloc failed"); 
	}

	// Reading 
	for (uint8_t test_run = 0; test_run < 1; ++test_run) { 
		FILE *test_file = fopen(VIDEO_FILE_PATH, "rb"); 
		if (test_file == NULL) { 
			ESP_LOGE(SD_TAG, "Unable to open file");
		}

		for (uint16_t i = 1; i<250; ++i) { 
			if (i == 1) {
				fread(item, 1024*16, 1, test_file); 
				continue;
			} else {
				fread(item2, 1024*16, 1, test_file); 
			}

			if (strcmp(item, item2)) {
				ESP_LOGW(SD_TAG, "Error when writing block"); 
			} else {
				ESP_LOGI(SD_TAG, "Block %d written correctly", i);
			}
		}
		fclose(test_file); 
	}

	// Writing 
	/*for (uint8_t test_run = 0; test_run < 1; ++test_run) { */
	/*	FILE *test_file = fopen(VIDEO_FILE_PATH, "w"); */
	/*	esp_fill_random(item, 1024*16);*/
	/**/
	/*	int64_t start_time = esp_timer_get_time(); */
	/*	for (uint16_t i = 1; i<250; ++i) { // Write 16KB * 250 = 4MB*/
	/*		fwrite(item, 1024*16, 1, test_file); */
	/*	}*/
	/*	int64_t end_time = esp_timer_get_time(); */
	/*	test_result = 4./(((float)(end_time-start_time))/1000000); */
	/*	test_average += test_result; */
	/*	ESP_LOGI(SD_TAG, "Writing 4MB in %lld milliseconds (%.2fMB/s)", (end_time-start_time)/1000, test_result);*/
	/*	fclose(test_file); */
	/*}*/

	test_average /= 8;
	ESP_LOGI(SD_TAG, "Average write speed after 8 test runs: %.2fMB/s", test_average); 

	// Unmount partition and disable SDMMC peripheral
	esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
	ESP_LOGI(SD_TAG, "Card unmounted");
	vTaskDelay(portMAX_DELAY);
}
