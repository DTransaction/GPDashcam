#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "global.h"
#include "gpio.h"

static void IRAM_ATTR button_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
	gpio_intr_disable(gpio_num);
    xQueueSendFromISR(gpio_event_queue, &gpio_num, NULL);
}

void init_gpio() { 
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_conf.pin_bit_mask = ((1ULL<<CONFIG_GPIO_INPUT0) | (1ULL<<CONFIG_GPIO_INPUT1));

    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pull_up_en = 1;
    gpio_config(&gpio_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(CONFIG_GPIO_INPUT0, button_handler, (void*) CONFIG_GPIO_INPUT0);
    gpio_isr_handler_add(CONFIG_GPIO_INPUT1, button_handler, (void*) CONFIG_GPIO_INPUT1);
}
