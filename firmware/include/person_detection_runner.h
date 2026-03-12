#ifdef __cplusplus
extern "C" {
#endif

void tinyml_init(void);
bool run_stop_detection(const int8_t* ml_input);

// void rgb565_to_rgb96(const uint16_t *src,
//                             int src_w,
//                             int src_h,
//                             int8_t *dst);

// void rgb565_to_rgb96_char(
//     const unsigned char *src,
//     int src_w,
//     int src_h,
//     int8_t *dst);
// void draw_status_bar_rgb565(camera_fb_t* fb, uint16_t color);
// // void run_test_image(void);

// void rgb888_to_rgb96(
//     const unsigned char *src,
//     int src_w,
//     int src_h,
//     int8_t *dst);

// void dump_tensor(int8_t *tensor);

void resize_and_rgb565_to_rgb888(
    const uint16_t *src,
    int width,
    int height,
    int8_t *dst);

void resize_and_rgb565_to_rgb888_2(
    const uint8_t *src,
    int src_width,
    int src_height,
    int8_t *dst);
// void resizeNearestNeighbor(uint16_t* src, int srcW, int srcH, 
//                           uint16_t* dst, int dstW, int dstH, int channels); 

// void rgb565_bilinear_resize_to_tensor(
//     const uint16_t *src,
//     int src_w,
//     int src_h,
//     int8_t *dst);

// void crop_resize_rgb565_bilinear_to_tensor(
//     const uint16_t *src,
//     int8_t *dst);

#ifdef __cplusplus
}
#endif
