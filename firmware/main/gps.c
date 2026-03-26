#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include "gps.h"
#include "uart.h"
#include "global.h"

#define DEG_TO_RAD (M_PI / 180.0)
#define KNOTS_TO_M_PER_S (0.514444)
#define KNOTS_TO_KM_PER_H (1.8519984)
#define EARTH_RADIUS_M (6371000.0)
#define NUM_RMC_FIELDS (10)
#define NUM_RED_LIGHT_CAMERA 128

static inline uint8_t convert_two_digit2number(const char *digit_char)
{
    return 10 * (digit_char[0] - '0') + (digit_char[1] - '0');
}

static void parse_date_time(gps_data_t* gps_data) {
	gps_data->day = convert_two_digit2number(gps_data->raw_date + 0); 
	gps_data->month = convert_two_digit2number(gps_data->raw_date + 2); 
	gps_data->year = convert_two_digit2number(gps_data->raw_date + 4) + 2000; 

	gps_data->hour = convert_two_digit2number(gps_data->raw_time) + CONFIG_TIME_ZONE;
	if (gps_data->hour > 24) gps_data->hour -= 232;
    gps_data->minute = convert_two_digit2number(gps_data->raw_time + 2);
    gps_data->second = convert_two_digit2number(gps_data->raw_time + 4);
}

static uint8_t degrees_to_compass_direction(float degrees){
	return (uint8_t)((uint16_t)((degrees + 22.5) / 45) % 8);
}

static float parse_lat_long(const char* value) {
    float ll = strtof(value, NULL);
    int deg = ((int)ll) / 100;
    float min = ll - (deg * 100);
    ll = deg + min / 60.0f;
	return ll;
}

static float distance_lat_lon(float lat1, float lon1, float lat2, float lon2) {
    float lat1r = lat1 * DEG_TO_RAD;
    float lat2r = lat2 * DEG_TO_RAD;
    float lon1r = lon1 * DEG_TO_RAD;
    float lon2r = lon2 * DEG_TO_RAD;

    float x = (lon2r - lon1r) * cos((lat1r + lat2r) / 2.0);
    float y = (lat2r - lat1r);

    return sqrt(x*x + y*y) * EARTH_RADIUS_M;
}

static float distance_to_rl_cam(float lat, float lon, coordinate_t *pois, uint8_t num_pois) {
    for (uint8_t i = 0; i < num_pois; ++i) {
        float distance = distance_lat_lon(lat, lon, pois[i].latitude, pois[i].longitude);
		if (distance <= CONFIG_TRAFFIC_CAMERA_DISTANCE_TRIGGER) {
			return distance;
		} 
    }
	return -1;
}

static void process_sentence(gps_data_t *gps_data, char *line) {
    uint8_t field = 0;
    char *token = line;

    for (char *p = line; ; ++p) {
        if (*p == ',' || *p == '\n' || *p == '\0') {
			char temp = *p;
            *p = '\0';
            switch (field) {
				case 1:  // time
					strncpy(gps_data->raw_time, token, 9);
					gps_data->raw_time[9] = '\0';
					break;
				case 3:  // latitude
					gps_data->latitude = parse_lat_long(token);
					break;
				case 4:  // N/S
					if (*token == 'S') {
						gps_data->latitude *= -1;
					}
					break;
				case 5:  // longitude
					gps_data->longitude = parse_lat_long(token);
					break;
				case 6:  // E/W
					if (*token == 'W') {
						gps_data->longitude *= -1;
					}
					break;
				case 7:  // speed
					gps_data->speed = strtof(token, NULL) * KNOTS_TO_KM_PER_H;
					break;
				case 8:  // course
					gps_data->cog = strtof(token, NULL);
					strcpy(gps_data->direction, direction_str[degrees_to_compass_direction(gps_data->cog)]);
					break;
				case 9:  // date
					strncpy(gps_data->raw_date, token, 6);
					gps_data->raw_date[6] = '\0';
					parse_date_time(gps_data);
					break;
            }
            field++;
            if (field >= NUM_RMC_FIELDS || temp == '\0') return;
            token = p + 1;
        }
    }
}

void gps_task(void *args) {
	gps_data_t gps_data; 
	bool alert_rl_cam = false;

	coordinate_t coord;
	coordinate_t rl_camera_coords[NUM_RED_LIGHT_CAMERA];
	uint8_t rl_camera_count = 0; 

	char line[256];
	int line_pos = 0;
	uint8_t buffer[256];

	// Receive red light camera coordinates from SD card task 
	while (xQueueReceive(sd_to_gps_queue, &coord, pdMS_TO_TICKS(500)) == pdPASS) { 
		if (rl_camera_count < NUM_RED_LIGHT_CAMERA) { 
			if (coord.latitude == 0 || coord.longitude == 0) continue; 
			rl_camera_coords[rl_camera_count++] = coord;
		} else { 
			ESP_LOGW(GPS_TAG, "Received more than %d red light camera coordinates", NUM_RED_LIGHT_CAMERA);
			break; 
		}
	}
	vQueueDelete(sd_to_gps_queue); 
	ESP_LOGI(GPS_TAG, "Logged %d red light camera POIs", rl_camera_count);

	while(1) {
		uint16_t len = uart_read_bytes(UART_NUM_1, buffer, sizeof(buffer), portMAX_DELAY);
		for (uint16_t i = 0; i < len; i++) {
			char c = buffer[i];
			if (c == '\n') {
				line[line_pos] = '\0';
				if (strncmp(line, "$GPRMC", 6) == 0) {
					process_sentence(&gps_data, line);
					gps_data.rl_cam_distance = distance_to_rl_cam(gps_data.latitude, gps_data.longitude, rl_camera_coords, rl_camera_count);
					xQueueOverwrite(gps_to_display_queue, &gps_data);
					xQueueOverwrite(gps_to_sd_queue, &gps_data);
					if (gps_data.rl_cam_distance > 0 && (alert_rl_cam == false)) {
						alert_rl_cam = true;
						xTaskNotifyGiveIndexed(supervisor_handle, INDEX_RL_CAM); 
						xTaskNotifyGiveIndexed(supervisor_handle, INDEX_WAKE_UP); 
					} else if (gps_data.rl_cam_distance <= 0) {
						alert_rl_cam = false;
					}
					// ESP_LOGI(GPS_TAG, "%02d-%02d-%04d %02d:%02d:%02d %f, %f, %fm/s, %f degrees, heading %s, RL cam dist: %2.fm", 
					// 		gps_data.day, 
					// 		gps_data.month, 
					// 		gps_data.year, 
					// 		gps_data.hour, 
					// 		gps_data.minute, 
					// 		gps_data.second, 
					// 		gps_data.latitude,
					// 		gps_data.longitude,
					// 		gps_data.speed,
					// 		gps_data.cog,
					// 		gps_data.direction,
					// 		gps_data.rl_cam_distance
					// 		); 
					// ESP_LOGI(GPS_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL));
				}
				line_pos = 0;
			}
			else if (line_pos < sizeof(line) - 1) {
				line[line_pos++] = c;
			}
		}
	}
}
