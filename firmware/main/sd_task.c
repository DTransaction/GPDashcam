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
	host.max_freq_khz = 20000; 

    // Options for mounting the filesystem.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };


	ESP_LOGI(SD_TAG, "Initializing SD card");
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

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
	ret = esp_vfs_fat_sdcard_format(MOUNT_POINT, card);
	if (ret != ESP_OK) {
		ESP_LOGE(SD_TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
		return;
	}
    ESP_LOGI(SD_TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

	float test_result; 
	float test_average = 0; 
	char *first_block = (char *)malloc(1024*16); // 16KB
	char *current_block = (char *)malloc(1024*16); // 16KB
	if (first_block == NULL) { 
		ESP_LOGI(SD_TAG, "Malloc failed"); 
	}
	if (current_block == NULL) { 
		ESP_LOGI(SD_TAG, "Malloc failed"); 
	}

	for (uint8_t test_run = 0; test_run < 8; ++test_run) { 
		FILE *test_file = fopen(VIDEO_FILE_PATH, "wb"); 
		if (test_file == NULL) { 
			ESP_LOGE(SD_TAG, "Unable to open file");
		}
		esp_fill_random(first_block, 1024*16);

		int64_t start_time = esp_timer_get_time(); 
		// Writing 
		for (uint16_t i = 0; i<250; ++i) { // Write 16KB * 250 = 4MB
			fwrite(first_block, 1024*16, 1, test_file); 
		}
		int64_t end_time = esp_timer_get_time(); 
		test_result = 4./(((float)(end_time-start_time))/1000000); 
		test_average += test_result; 
		fclose(test_file);

		// Reading
		test_file = fopen(VIDEO_FILE_PATH, "rb"); 
		uint8_t bad_block_flag = 0; 
		for (uint16_t i = 0; i<250; ++i) { 
			fread(current_block, 1024*16, 1, test_file); 

			if (strcmp(current_block, first_block)) {
				ESP_LOGE(SD_TAG, "Error when writing block %d", i); 
				bad_block_flag = 1; 
			} 
		}

		if (bad_block_flag) { 
			ESP_LOGW(SD_TAG, "Error when writing blocks to file"); 
		} else {
			ESP_LOGI(SD_TAG, "Wrote 4MB in %lld milliseconds (%.2fMB/s) with no errors", (end_time-start_time)/1000, test_result);
		}

		fclose(test_file); 
	}

	test_average /= 8;
	ESP_LOGI(SD_TAG, "Average write speed after 8 test runs: %.2fMB/s", test_average); 

	// Unmount partition and disable SDMMC peripheral
	esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
	ESP_LOGI(SD_TAG, "Card unmounted");
	vTaskDelay(portMAX_DELAY);
}
