#include "driver/uart.h"
#include "uart.h"

void init_uart() { 
	const uart_config_t uart_config = {
		.baud_rate = BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUFFER_SIZE, 0, 0, NULL, 0));
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, CONFIG_RX_PIN_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_flush(UART_NUM_1); 
}
