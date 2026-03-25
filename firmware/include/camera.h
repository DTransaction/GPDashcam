#ifndef CAMERA_H
#define CAMERA_H

#define CAMERA_TAG "CAMERA_TASK"
esp_err_t init_camera();

extern camera_fb_t *latest_frame;
extern SemaphoreHandle_t frame_mutex;

void camera_task(void *args);

#endif 
