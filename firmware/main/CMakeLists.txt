idf_component_register(
	SRCS 
	"main.c"
	"accelerometer_task.c"
	"gps_task.c"
	"display_task.c"
	"sd_task.c"
	"camera.c"
	"person_detection_runner.cpp"
	INCLUDE_DIRS "../include"
	REQUIRES fatfs esp_lcd esp_timer tinyml_person
	WHOLE_ARCHIVE)
set(COMPONENT_KCONFIG main/Kconfig)