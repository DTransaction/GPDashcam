#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sd.h"

#define MAIN_TAG "MAIN_TASK"

void app_main(void) {
	// Create tasks 
	ESP_LOGI(MAIN_TAG, "Creating tasks");
    xTaskCreate(sd_task, SD_TAG, 4096, NULL, 4, NULL);

	while(1){ 
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

