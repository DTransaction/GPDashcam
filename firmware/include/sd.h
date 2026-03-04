/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include "driver/sdmmc_host.h"

#ifndef SD_H
#define SD_H

#define MAX_CHAR_SIZE (128)
#define MOUNT_POINT "/sdcard"
#define SD_TAG "SD_TASK"

void mount_sd(sdmmc_card_t* card);
void sd_task(void *args);

#endif // SD_H
