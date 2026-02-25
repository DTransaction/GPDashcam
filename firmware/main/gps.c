#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "gps.h"

static inline uint8_t convert_two_digit2number(const char *digit_char)
{
    return 10 * (digit_char[0] - '0') + (digit_char[1] - '0');
}

static uint8_t get_days_of_month(gps_data_t* gps_data){
	switch (gps_data->month) { 
		case 1: 
			return 31; 
		case 2: 
			return 28; 
		case 3: 
			return 31;
		case 4: 
			return 30;
		case 5: 
			return 31;
		case 6: 
			return 30; 
		case 7: 
			return 31;
		case 8: 
			return 31; 
		case 9: 
			return 30; 
		case 10: 
			return 31; 
		case 11: 
			return 30; 
		case 12: 
			return 31; 
	}
	return 0; 
}

static void parse_date_time(gps_data_t* gps_data) {
	gps_data->day = convert_two_digit2number(gps_data->raw_date + 0); 
	gps_data->month = convert_two_digit2number(gps_data->raw_date + 2); 
	gps_data->year = convert_two_digit2number(gps_data->raw_date + 4) + 2000; 

	int8_t adjusted_time = convert_two_digit2number(gps_data->raw_time) + TIME_ZONE;
	if(adjusted_time < 0) { // Wrap around midnight 
		gps_data-> hour = adjusted_time + 24; 
		if(gps_data->day == 1){ // Rollback the month
			gps_data->day = get_days_of_month(gps_data); 
			gps_data->month -= 1; 
		} else { 
			gps_data->day -= 1; 
		}
	}
    gps_data->minute = convert_two_digit2number(gps_data->raw_time + 2);
    gps_data->second = convert_two_digit2number(gps_data->raw_time + 4);
}

static uint8_t degrees_to_compass_direction(float degrees){
	return (uint8_t)((uint16_t)((degrees + 22.5) / 45) % 8);
}

static float parse_lat_long(const char* value, const char* ll_direction) {
    float ll = strtof(value, NULL);
    int deg = ((int)ll) / 100;
    float min = ll - (deg * 100);
    ll = deg + min / 60.0f;
	if(ll_direction[0] == 'N' || ll_direction[0] == 'E') return ll;
	return -ll;
}

static void parse_fields(char *fields[], gps_data_t *gps_data){ 
	// Parse for time and date 
	if(fields[1] && fields[1][0] != '\0' && fields[9] && fields[9][0] != '\0') {
		strncpy(gps_data->raw_time, fields[1], 9); 
		gps_data->raw_time[9] = '\0';
		strncpy(gps_data->raw_date, fields[9], 6); 
		gps_data->raw_date[6] = '\0';
		parse_date_time(gps_data); 
	}
	// Parse for latitude and longitude 
	if(fields[3] && fields[3][0] != '\0' &&
	   fields[4] && fields[4][0] != '\0' &&
	   fields[5] && fields[5][0] != '\0' &&
	   fields[6] && fields[6][0] != '\0') {
		gps_data->latitude = parse_lat_long(fields[3], fields[4]); 
		gps_data->longitude = parse_lat_long(fields[5], fields[6]); 
	}
	// Parse for speed 
	if(fields[7] && fields[7][0] != '\0') gps_data->speed = strtof(fields[7], NULL) * 0.514444; 
	// Parse for heading 
	if(fields[8] && fields[8][0] != '\0') {
		gps_data->cog = strtof(fields[8], NULL);
		gps_data->direction = direction_str[degrees_to_compass_direction(gps_data->cog)]; 
	} else {
		gps_data->cog = 0;
		gps_data->direction = ""; 
	}
}

void gps_task(void *args) {
	QueueHandle_t *queues = (QueueHandle_t *)args; 
	QueueHandle_t gps_to_display_queue = queues[0]; 
	QueueHandle_t gps_to_sd_queue = queues[1]; 
	/*QueueHandle_t *gps_to_display_queue = (QueueHandle_t *)args[0]; */
	/*QueueHandle_t *gps_to_sd_queue = (QueueHandle_t *)args[1]; */

    char *buffer = malloc(RX_BUFFER_SIZE + 1); // +1 to ensure '\0' can always be appended
	gps_data_t *gps_data = malloc(sizeof(gps_data_t)); 
	char *fields[RMC_SIZE]; 
	char *sentence_remainder; 

	ESP_LOGI(GPS_TAG, "Initialized"); 

	while(1) {
		uint8_t len = uart_read_bytes(UART_NUM_1, buffer, RX_BUFFER_SIZE, pdMS_TO_TICKS(100));
		if(len == 0) continue;

		char *sentence = buffer;
		// Loop through all received bytes 
		for(uint8_t i=0; i<len; ++i) {
			if(buffer[i]=='\n') {
				// Replace detected newline with the string complete character 
				// Now sentence has the line 
				buffer[i]='\0';

				// Check for $GPRMC that contains data 
				if (sentence[5]!='C' || strlen(sentence)<26) { 
					sentence = buffer+i+1; // Shift line read pointer over 
					continue; 
				}

				uint8_t field_count = 0; 
				memset(gps_data, 0, sizeof(gps_data_t));
				sentence_remainder = sentence; 
				// Parse RMC sentence into an array using comma separation 
				// Empty fields are still recorded into array to keep consistent
				// indexing
				for (char *p = sentence; ; ++p) {
					if (*p == ',' || *p == '\0') {
						// Temporarily terminate current field
						char temp = *p;
						*p = '\0';

						// store pointer to start of field (even if empty)
						fields[field_count++] = sentence_remainder;

						// restore character if not end of string
						if (temp == '\0' || field_count >= RMC_SIZE)
							break;

						// next field starts after comma
						sentence_remainder = p + 1;
					}
				}

				parse_fields(fields, gps_data); 
				xQueueOverwrite(gps_to_display_queue, gps_data);
				xQueueOverwrite(gps_to_sd_queue, gps_data);
				ESP_LOGI(GPS_TAG, "%02d-%02d-%04d %02d:%02d:%02d %f, %f, %fm/s, %f degrees, heading %s", 
						gps_data->day, 
						gps_data->month, 
						gps_data->year, 
						gps_data->hour, 
						gps_data->minute, 
						gps_data->second, 
						gps_data->latitude,
						gps_data->longitude,
						gps_data->speed,
						gps_data->cog,
						gps_data->direction
						); 
				/*ESP_LOGI(GPS_TAG, "High water mark: %d", uxTaskGetStackHighWaterMark(NULL)); ;*/
				vTaskDelay(pdMS_TO_TICKS(100));
			}
		}
	}
    free(buffer);
	free(gps_data); 
}
