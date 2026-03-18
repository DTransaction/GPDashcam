#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "accelerometer.h"
#include "gps.h"
#include "display.h"
#include "global.h"

#define NUM_DISPLAY_MODES 4

typedef enum {
	ACCELEROMETER,
	GPS,
	WIFI, 
	ML
} DisplayMode;

// 5x7 font table (ASCII 0x20–0x7F)
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x72,0x49,0x49,0x49,0x46}, // '2'
    {0x21,0x41,0x49,0x4D,0x33}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x31}, // '6'
    {0x41,0x21,0x11,0x09,0x07}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x46,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x59,0x09,0x06}, // '?'
    {0x3E,0x41,0x5D,0x59,0x4E}, // '@'
    {0x7C,0x12,0x11,0x12,0x7C}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 'F'
    {0x3E,0x41,0x41,0x51,0x73}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x03,0x04,0x78,0x04,0x03}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x03,0x07,0x08,0x00}, // '`'
    {0x20,0x54,0x54,0x78,0x40}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x40,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x4C,0x90,0x90,0x90,0x7C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x77,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x02,0x01,0x02,0x04,0x02}, // '~'
};

static DisplayMode next_mode(DisplayMode mode) { 
	if (mode >= NUM_DISPLAY_MODES) return 0;
	else return (mode + 1);
}

static DisplayMode prev_mode(DisplayMode mode) { 
	if (mode == 0) return NUM_DISPLAY_MODES - 1;
	else return (mode - 1);
}

void init_display(esp_lcd_panel_handle_t *panel) { 
	// Initialize display 
    ESP_LOGI(DISPLAY_TAG, "Install SSD1306 panel driver");
	esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_ADDR,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel));
	ESP_ERROR_CHECK(esp_lcd_panel_init(*panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel, true));
}
// Display buffer (1-bit monochrome)
static uint8_t oled_buffer[LCD_H * LCD_V / 8];

// clears buffer
static void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

static void draw_char(uint8_t x, uint8_t y, char c) {
    if (c < 32 || c > 127) return;
    const uint8_t *ch = font5x7[c - 32];
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t col = ch[i];
        for (uint8_t j = 0; j < 7; j++) {
            if (col & (1 << j)) {
                uint16_t byte_index = ((y + j) / 8) * LCD_H + (x + i);
                oled_buffer[byte_index] |= 1 << ((y + j) % 8);
            }
        }
    }
}

static void draw_string(uint8_t x, uint8_t y, const char *str) {
    uint8_t cursor_x = x;
    uint8_t cursor_y = y;
    while (*str) {
        if (*str == '\n') {
            cursor_x = x;
            cursor_y += 8;
        } else {
            draw_char(cursor_x, cursor_y, *str);
            cursor_x += 6;
        }
		++str; 
    }
}

void display_task(void *args) {
	DisplayMode display_mode = (DisplayMode)ML; 
    char buffer[BUFFER_SIZE];
	accel_data_t accel_data; 
	gps_data_t gps_data;

	uint32_t gpio_num;
	bool button0_pressed = false; 
	bool button1_pressed = false; 

	// TEMPORARY
	vTaskDelay(pdMS_TO_TICKS(500));
	xTaskNotifyGiveIndexed(camera_handle, INDEX_ML); // Notify camera to disable ML mode
    while (1) {
		// Button press receive and debouncing 
		// Delay acts as refresh rate delay
        if (xQueueReceive(gpio_event_queue, &gpio_num, pdMS_TO_TICKS(1000/CONFIG_REFRESH_RATE))) {
			vTaskDelay(pdMS_TO_TICKS(CONFIG_DEBOUNCE_TIME_MS));
			if (gpio_num) {
				if (display_mode == (DisplayMode)ML) {
					xTaskNotifyGiveIndexed(camera_handle, INDEX_ML); // Notify camera to disable ML mode
				}
				if (gpio_num == CONFIG_GPIO_INPUT0) {
					if ((gpio_get_level(CONFIG_GPIO_INPUT0) == 0) && !button0_pressed) { 
						display_mode = next_mode(display_mode); 
						button0_pressed = true; 
					} else if ((gpio_get_level(CONFIG_GPIO_INPUT0) == 1) && button0_pressed) { 
						button0_pressed = false; 
					} 
					gpio_intr_enable(CONFIG_GPIO_INPUT0);
				}
				else if (gpio_num == CONFIG_GPIO_INPUT1) {
					if ((gpio_get_level(CONFIG_GPIO_INPUT1) == 0) && !button1_pressed) { 
						display_mode = prev_mode(display_mode); 
						button1_pressed = 1; 
					} else if ((gpio_get_level(CONFIG_GPIO_INPUT1) == 1) && button1_pressed) { 
						button1_pressed = 0; 
					}
					gpio_intr_enable(CONFIG_GPIO_INPUT1);
				}
				if (display_mode == (DisplayMode)ML) {
					xTaskNotifyGiveIndexed(camera_handle, INDEX_ML); // Notify camera to enable ML mode
				}
			}
        }
		switch (display_mode) {
			case ACCELEROMETER:
				xQueueReceive(accel_to_display_queue, &accel_data, pdMS_TO_TICKS(500));
				snprintf(buffer, sizeof(buffer), 
					"A: %.2f %.2f %.2f\n"
					"Total mag: %.2f\n",
					accel_data.x, 
					accel_data.y, 
					accel_data.z,
					accel_data.total_magnitude
				);
				break;
			case GPS: 
				xQueueReceive(gps_to_display_queue, &gps_data, pdMS_TO_TICKS(500));
				snprintf(buffer, sizeof(buffer), 
					"Lat: %f\n"
					"Lon: %f\n"
					"RL cam dist: %.0fm\n"
					"Date: %02d-%02d-%04d\n"
					"Time: %02d:%02d:%02d", 
					gps_data.longitude,
					gps_data.latitude,
					gps_data.rl_cam_distance,
					gps_data.day, 
					gps_data.month, 
					gps_data.year,
					gps_data.hour, 
					gps_data.minute, 
					gps_data.second
				);
				break;
			case WIFI: 
				snprintf(buffer, sizeof(buffer),
					"SSID: %s\nPass: %s\n"
					"To download files,\n"
					"open your browser and\n"
					"go to 192.168.4.1", 
					CONFIG_ESP_WIFI_SSID, 
					CONFIG_ESP_WIFI_PASSWORD
				);
				break;
			case ML: 
				int8_t score = 0; 
				xQueueReceive(camera_to_display_queue, &score, pdMS_TO_TICKS(500)); 
				snprintf(buffer, sizeof(buffer),
					"ML Mode\n"
					"Stop sign score: %d",
					score
				);
				break;
			default: 
				display_mode = 0; 
				break; 
		}
		oled_clear();
		draw_string(0, 0, buffer);
		esp_lcd_panel_draw_bitmap(*panel, 0, 0, LCD_H, LCD_V, oled_buffer);
		// ESP_LOGI(DISPLAY_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
    }
}




