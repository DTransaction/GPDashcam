idf_component_register(
	SRCS 
	"main.c"
	"sd_task.c"
	INCLUDE_DIRS "../include"
	REQUIRES fatfs sd_card esp_timer
	WHOLE_ARCHIVE)
set(COMPONENT_KCONFIG main/Kconfig)
