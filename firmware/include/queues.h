#ifndef QUEUES_H
#define QUEUES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t accel_to_display_queue;
extern QueueHandle_t gps_to_display_queue; 
extern QueueHandle_t gps_to_sd_queue; 
extern QueueHandle_t camera_to_sd_queue; 

#endif
