idf_component_register(
	SRCS 
	"main.c"
	"accelerometer_task.c"
	"gps_task.c"
	"display_task.c"
	"sd.c"
	"camera.c"
	"softap.c"
	"file_server.c"
	INCLUDE_DIRS "../include"
	REQUIRES fatfs esp_lcd nvs_flash esp_http_server esp_wifi esp_event esp_netif vfs
	EMBED_FILES "favicon.ico" "upload_script.html")
set(COMPONENT_KCONFIG main/Kconfig)
