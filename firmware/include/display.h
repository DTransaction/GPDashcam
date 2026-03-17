#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_lcd_panel_io.h"

#define DISPLAY_TAG "DISPLAY_TASK"
#define I2C_ADDR 0x3C
#define LCD_H 128
#define LCD_V 64
#define LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define BUFFER_SIZE 256

void init_display(esp_lcd_panel_handle_t *panel);
void display_task(void *arg);

#endif // DISPLAY_H
