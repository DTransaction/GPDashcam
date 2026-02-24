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
#include "i2c_common.h"
#include "camera.h"


#define MAIN_TAG "MAIN_TASK"

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

void init_i2c(i2c_master_bus_handle_t* i2c_bus) { 
	i2c_master_bus_config_t bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = 0,
		.scl_io_num = CONFIG_SCL_PIN_NUM,
		.sda_io_num = CONFIG_SDA_PIN_NUM,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, i2c_bus));
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
void app_main(void) {

	ESP_LOGI(MAIN_TAG, "Initialize I2C bus");
	i2c_master_bus_handle_t i2c_bus;
	init_i2c(&i2c_bus); 

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

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_softap();
	ESP_ERROR_CHECK(example_start_file_server(MOUNT_POINT));

	ESP_LOGI(MAIN_TAG, "Create queues");
	QueueHandle_t accel_to_display_queue = xQueueCreate(1, sizeof(accel_data_t)); 
	QueueHandle_t gps_to_display_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	QueueHandle_t gps_to_sd_queue = xQueueCreate(1, sizeof(gps_data_t)); 
	QueueHandle_t camera_to_sd_queue = xQueueCreate(1, sizeof(camera_fb_t)); 

	// Done in this manner because we have flexible array members 
	i2c_task_args_t *accel_args = (i2c_task_args_t*)malloc(sizeof(i2c_task_args_t*) + 1*sizeof(QueueHandle_t*)); 
	i2c_task_args_t *display_args = (i2c_task_args_t*)malloc(sizeof(i2c_task_args_t*) + 2*sizeof(QueueHandle_t*)); 
	QueueHandle_t *gps_args[2] = {&gps_to_display_queue, &gps_to_sd_queue}; 
	QueueHandle_t *sd_args[2] = {&gps_to_sd_queue, &camera_to_sd_queue}; 
	QueueHandle_t *camera_args[1] = {&camera_to_sd_queue}; 

	// Populating task arguments
	accel_args->i2c_bus = &i2c_bus;
	accel_args->queues[0] = &accel_to_display_queue;

	display_args->i2c_bus = &i2c_bus; 
	display_args->queues[0] = &accel_to_display_queue;
	display_args->queues[1] = &gps_to_display_queue;

	ESP_LOGI(MAIN_TAG, "Creating tasks");
	xTaskCreatePinnedToCore(accelerometer_task, ACCEL_TAG, 2500, accel_args, 3, NULL, 1); 
	xTaskCreatePinnedToCore(gps_task, GPS_TAG, 4500, gps_args, 3, NULL, 1);
	xTaskCreatePinnedToCore(display_task, DISPLAY_TAG, 4096, display_args, 3, NULL, 1);
	xTaskCreatePinnedToCore(sd_task, SD_TAG, 4096, sd_args, 4, NULL, 1);
	xTaskCreatePinnedToCore(camera_task, CAMERA_TAG, 4096, camera_args, 5, NULL, 0); 

	vTaskSuspend(NULL); 

	ESP_LOGE(MAIN_TAG, "Main task exited unexpectedly"); 
	free(accel_to_display_queue); 
	free(gps_to_display_queue); 
	free(gps_to_sd_queue); 
	free(camera_to_sd_queue); 
	// Unmount partition and disable SDMMC peripheral
	esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
	ESP_LOGI(SD_TAG, "Card unmounted");
}

