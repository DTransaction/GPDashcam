/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char** names;
    const int* pins;
} pin_configuration_t;


void check_sd_card_pins(pin_configuration_t *config, const int pin_count);

#ifdef __cplusplus
}
#endif
