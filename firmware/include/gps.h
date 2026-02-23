#ifndef GPS_H
#define GPS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"

// -----------------------------------------------------------------------------
// Configuration constants
// -----------------------------------------------------------------------------
#define GPS_TAG "GPS_TASK"
#define BAUD_RATE 9600
#define QUEUE_SIZE 16
#define RMC_SIZE 13
#define TIME_ZONE -5
#define RX_BUFFER_SIZE 1024

// -----------------------------------------------------------------------------
// Direction strings (N, NE, E, SE, etc.)
// -----------------------------------------------------------------------------
static const char* direction_str[] = {
    "N",
    "NE",
    "E",
    "SE",
    "S",
    "SW",
    "W",
    "NW"
};

// -----------------------------------------------------------------------------
// RMC (Recommended Minimum Specific GNSS Data) structure
// -----------------------------------------------------------------------------
typedef struct {
    char     raw_time[10];
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    char     raw_date[7];
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    float    latitude;
    float    longitude;
    float    speed;
    float    cog;         // course over ground
    char*    direction;   // human-readable compass direction
} gps_data_t;

// -----------------------------------------------------------------------------
// Function declarations
// -----------------------------------------------------------------------------


/**
 * @brief FreeRTOS task to read GPS NMEA sentences and parse RMC data.
 *
 * The task initializes UART1, reads incoming NMEA data,
 * splits the RMC sentence into comma-separated fields (including empty ones),
 * and logs the parsed data.
 */
void gps_task(void *arg);

#endif // GPS_H

