/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "sd_test_io.h"

const static char *TAG = "SD_TEST";

#define ADC_ATTEN_DB              ADC_ATTEN_DB_12
#define GPIO_INPUT_PIN_SEL(pin)   (1ULL<<pin)

static uint32_t get_cycles_until_pin_level(int i, int level, int timeout) {
    uint32_t start = esp_cpu_get_cycle_count();
    while(gpio_get_level(i) == !level && esp_cpu_get_cycle_count() - start < timeout) {
        ;
    }
    uint32_t end = esp_cpu_get_cycle_count();
    return end - start;
}

void check_sd_card_pins(pin_configuration_t *config, const int pin_count)
{
    ESP_LOGI(TAG, "Testing SD pin connections and pullup strength");
    gpio_config_t io_conf = {};
    for (int i = 0; i < pin_count; ++i) {
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
        io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL(config->pins[i]);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
    }

    printf("\n**** PIN recovery time ****\n\n");

    for (int i = 0; i < pin_count; ++i) {
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_set_level(config->pins[i], 0);
        usleep(100);
        gpio_set_level(config->pins[i], 1);
        uint32_t cycles = get_cycles_until_pin_level(config->pins[i], 1, 10000);
        printf("PIN %2d %3s  %"PRIu32" cycles\n", config->pins[i], config->names[i], cycles);
    }

    printf("\n**** PIN recovery time with weak pullup ****\n\n");

    for (int i = 0; i < pin_count; ++i) {
        gpio_set_direction(config->pins[i], GPIO_MODE_INPUT_OUTPUT_OD);
        gpio_pullup_en(config->pins[i]);
        gpio_set_level(config->pins[i], 0);
        usleep(100);
        gpio_set_level(config->pins[i], 1);
        uint32_t cycles = get_cycles_until_pin_level(config->pins[i], 1, 10000);
        printf("PIN %2d %3s  %"PRIu32" cycles\n", config->pins[i], config->names[i], cycles);
        gpio_pullup_dis(config->pins[i]);
    }
}
