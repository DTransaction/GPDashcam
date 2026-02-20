/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef SD_EXAMPLE_H_
#define SD_EXAMPLE_H_

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum character buffer size for file operations */
#define MAX_CHAR_SIZE    128

/** @brief Default SD card mount point */
#define MOUNT_POINT "/sdcard"

/** @brief Determine if UHS-I mode is enabled */
#define IS_UHS1    (CONFIG_SDMMC_SPEED_UHS_I_SDR50 || CONFIG_SDMMC_SPEED_UHS_I_DDR50)

#define SD_TAG "SD_TASK"

void mount_sd(); 

/**
 * @brief SD card main task: initializes, mounts, reads/writes test files, and unmounts
 *
 * @param args Optional argument pointer for FreeRTOS task (unused)
 */
void sd_task(void *args);

#ifdef __cplusplus
}
#endif

#endif /* SD_EXAMPLE_H_ */
