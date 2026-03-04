#include "i2c.h"
#include "driver/i2c_master.h"

void init_i2c(i2c_master_bus_handle_t *i2c_bus) { 
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
