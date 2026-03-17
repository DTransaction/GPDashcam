#ifndef CAMERA_H
#define CAMERA_H

#define CAMERA_TAG "CAMERA_TASK"
esp_err_t init_camera();
void camera_task(void *args);

#endif 
