idf_component_register(
	SRCS 
	"main.c"
	"accelerometer.c"
	"gps.c"
	"display.c"
	"sd.c"
	"camera.c"
	"softap.c"
	"file_server.c"
	"uart.c"
	"i2c.c"
	"gpio.c"
	"stop_detection_runner.cpp"
	INCLUDE_DIRS "../include"
	REQUIRES fatfs esp_lcd nvs_flash esp_http_server esp_wifi esp_event esp_netif vfs tinyml_stop esp_timer
	EMBED_FILES "favicon.ico" "upload_script.html")
set(COMPONENT_KCONFIG main/Kconfig)
