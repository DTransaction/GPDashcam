#ifndef GPS_H
#define GPS_H

#include "esp_system.h"
#include "esp_log.h"
#include "string.h"

#define GPS_TAG "GPS_TASK"
#define RMC_SIZE (13)
#define TIME_ZONE (-5)

// Direction strings (N, NE, E, SE, etc.)
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

// RMC structure
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

void gps_task(void *arg);

#endif // GPS_H

