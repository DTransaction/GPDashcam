#ifndef DISPLAY_H
#define DISPLAY_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Defines 
#define DISPLAY_TAG "DISPLAY_TASK"
#define I2C_ADDR                0x3C
#define LCD_H                   128
#define LCD_V                   64
#define LCD_PIXEL_CLOCK_HZ      (400 * 1000)
#define BUFFER_SIZE 128

// Public function declarations
void display_task(void *arg);

#endif // DISPLAY_H
