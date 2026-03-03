#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "softap.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
// #include "sdmmc_cmd.h"

#include "file_server.h"
#include "accelerometer.h"
#include "gps.h"
#include "display.h"
#include "sd.h"
#include "camera.h"
#include "queues.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#define MAIN_TAG "MAIN_TASK"
#define SUPERVISOR_TAG "SUPERVISOR_TASK"

TaskHandle_t supervisor_handle; 
TaskHandle_t accel_handle; 
TaskHandle_t gps_handle; 
TaskHandle_t display_handle; 
TaskHandle_t sd_handle; 
TaskHandle_t camera_handle; 
QueueHandle_t accel_to_display_queue; 
QueueHandle_t gps_to_display_queue; 
QueueHandle_t gps_to_sd_queue; 
QueueHandle_t camera_to_sd_queue; 
i2c_master_bus_handle_t i2c_bus;
i2c_master_dev_handle_t i2c_accel_handle;
esp_lcd_panel_handle_t *panel;

void mount_sd(sdmmc_card_t* card) { 
	esp_err_t ret;
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20 MHz

	// Options for mounting the filesystem.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	ESP_LOGI(SD_TAG, "Initializing SD card");
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
	slot_config.width = 4;
	slot_config.clk = CONFIG_CLK_PIN_NUM;
	slot_config.cmd = CONFIG_CMD_PIN_NUM;
	slot_config.d0 = CONFIG_D0_PIN_NUM;
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
	// sdmmc_card_print_info(stdout, card);
}

void init_i2c() { 
	i2c_master_bus_config_t bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = 0,
		.scl_io_num = CONFIG_SCL_PIN_NUM,
		.sda_io_num = CONFIG_SDA_PIN_NUM,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));
}

void init_uart() { 
	QueueHandle_t uart_queue;
	const uart_config_t uart_config = {
		.baud_rate = BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUFFER_SIZE, 0, QUEUE_SIZE, &uart_queue, 0));
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, CONFIG_RX_PIN_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_flush(UART_NUM_1); 
}

static camera_config_t camera_config = {
	.pin_reset		= CONFIG_CAM_PIN_RESET,
	.pin_xclk		= CONFIG_CAM_PIN_XCLK,
	.pin_sccb_sda	= CONFIG_CAM_PIN_SIOD,
	.pin_sccb_scl	= CONFIG_CAM_PIN_SIOC,
	.pin_d7		  = CONFIG_CAM_PIN_D7,
	.pin_d6		  = CONFIG_CAM_PIN_D6,
	.pin_d5		  = CONFIG_CAM_PIN_D5,
	.pin_d4		  = CONFIG_CAM_PIN_D4,
	.pin_d3		  = CONFIG_CAM_PIN_D3,
	.pin_d2		  = CONFIG_CAM_PIN_D2,
	.pin_d1		  = CONFIG_CAM_PIN_D1,
	.pin_d0		  = CONFIG_CAM_PIN_D0,
	.pin_vsync		= CONFIG_CAM_PIN_VSYNC,
	.pin_href		= CONFIG_CAM_PIN_HREF,
	.pin_pclk		= CONFIG_CAM_PIN_PCLK,
	.xclk_freq_hz	= CONFIG_XCLK_FREQ_MHZ * 1000000,
	.ledc_timer	  = LEDC_TIMER_0,
	.ledc_channel	= LEDC_CHANNEL_0,
	.pixel_format	= PIXFORMAT_JPEG,
	.frame_size	  = FRAMESIZE_QHD,
	.jpeg_quality	= CONFIG_JPEG_QUALITY,
	.fb_count		= CONFIG_FB_COUNT
};

static esp_err_t init_camera(void)
{
	esp_err_t err = esp_camera_init(&camera_config);
	if (err != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Camera init failed: %s", esp_err_to_name(err));
	} else {
		ESP_LOGI(CAMERA_TAG, "Camera initialized successfully");
	}
	return err;
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

void init_accel(void) { 
	// Initialize accelerometer and add to bus 
    i2c_device_config_t i2c_accel_conf = {
        .scl_speed_hz = I2C_FREQUENCY,
        .device_address = I2C_ACCEL_ADDR,
    };
    if (i2c_master_bus_add_device(i2c_bus, &i2c_accel_conf, &i2c_accel_handle) != ESP_OK) {
        return;
    }
}

void supervisor_task(void *args) { 
	while (1) { 
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
		ESP_LOGI("SUPERVISOR_TASK", "Notifying camera and display"); 
		ESP_LOGI("SUPERVISOR_TASK", "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
	}
}

void app_main(void) {
	panel = malloc(sizeof(esp_lcd_panel_handle_t));
	ESP_LOGI(MAIN_TAG, "Initialize I2C bus");
	init_i2c(); 

	ESP_LOGI(MAIN_TAG, "Initialize UART1");
	init_uart(); 

	ESP_LOGI(MAIN_TAG, "Initialize camera"); 
	if (init_camera() != ESP_OK) {
		ESP_LOGE(CAMERA_TAG, "Failed to initialize camera");
		return;
	}

	ESP_LOGI(MAIN_TAG, "Initialize and mount SD card");
	sdmmc_card_t *card = malloc(sizeof(sdmmc_card_t));
	mount_sd(card); 

	ESP_LOGI(MAIN_TAG, "Initialize display"); 
	init_accel();
	init_display(panel); 

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_softap();
	ESP_ERROR_CHECK(example_start_file_server(MOUNT_POINT));

	ESP_LOGI(MAIN_TAG, "Create queues");
	accel_to_display_queue = xQueueCreate(1, sizeof(accel_data_t)); 
	gps_to_display_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	gps_to_sd_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	camera_to_sd_queue = xQueueCreate(CONFIG_FB_COUNT, sizeof(camera_fb_t*)); 

	if (!accel_to_display_queue) ESP_LOGE(MAIN_TAG, "Accel to display queue creation failed"); 
	if (!gps_to_display_queue) ESP_LOGE(MAIN_TAG, "GPS to display queue creation failed"); 
	if (!gps_to_sd_queue) ESP_LOGE(MAIN_TAG, "GPS to SD queue creation failed"); 
	if (!camera_to_sd_queue) ESP_LOGE(MAIN_TAG, "Camera to SD queue creation failed"); 

	xTaskCreatePinnedToCore(supervisor_task, SUPERVISOR_TAG, 2048, NULL, 6, &supervisor_handle, 1);
	ESP_LOGI(MAIN_TAG, "Supervisor task created");
	xTaskCreatePinnedToCore(display_task, DISPLAY_TAG, 4096, NULL, 3, &display_handle, 1);
	ESP_LOGI(MAIN_TAG, "Display task created");
	xTaskCreatePinnedToCore(accelerometer_task, ACCEL_TAG, 4500, NULL, 3, &accel_handle, 1); 
	ESP_LOGI(MAIN_TAG, "Accelerometer task created");
	xTaskCreatePinnedToCore(gps_task, GPS_TAG, 4500, NULL, 3, &gps_handle, 1);
	ESP_LOGI(MAIN_TAG, "GPS task created");
	xTaskCreatePinnedToCore(camera_task, CAMERA_TAG, 4096, NULL, 5, &camera_handle, 0); 
	ESP_LOGI(MAIN_TAG, "Camera task created");
	xTaskCreatePinnedToCore(sd_task, SD_TAG, 4096, NULL, 4, &sd_handle, 1);
	ESP_LOGI(MAIN_TAG, "SD task created");

	vTaskSuspend(NULL); // Can't continue unless another task calls vTaskResume with this task handle
}

