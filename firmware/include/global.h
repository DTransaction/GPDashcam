#ifndef GLOBAL_H
#define GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"

#define INDEX_IMPACT (0) 
#define INDEX_ML (1)

extern TaskHandle_t supervisor_handle; 
extern TaskHandle_t accel_handle; 
extern TaskHandle_t gps_handle; 
extern TaskHandle_t display_handle; 
extern TaskHandle_t sd_handle; 
extern TaskHandle_t camera_handle; 
extern QueueHandle_t accel_to_display_queue;
extern QueueHandle_t gps_to_display_queue; 
extern QueueHandle_t gps_to_sd_queue; 
extern QueueHandle_t sd_to_gps_queue; 
extern QueueHandle_t camera_to_sd_queue; 
extern QueueHandle_t gpio_event_queue; 

extern i2c_master_bus_handle_t i2c_bus;
extern i2c_master_dev_handle_t i2c_accel_handle;
extern esp_lcd_panel_handle_t *panel;
#endif
