#ifdef __cplusplus
extern "C" {
#endif

void tinyml_init(void);
float run_person_detection(const uint8_t* ml_gray);
void rgb565_to_gray96(const uint16_t *src,
                            int src_w,
                            int src_h,
                            uint8_t *dst);
void draw_status_bar_rgb565(camera_fb_t* fb, uint16_t color);


#ifdef __cplusplus
}
#endif
