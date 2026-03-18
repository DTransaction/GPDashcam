#ifdef __cplusplus
extern "C" {
#endif

void tinyml_init(void);

int8_t run_stop_detection(
    const uint8_t *src_bytes,
    int src_width,
    int src_height
);

void resize_and_rgb565_to_rgb888(
    const uint8_t *src_bytes,
    int src_width,
    int src_height,
    int8_t *dst);

void rgb565_to_rgb888(
    const uint8_t *src,
    int src_width,
    int src_height,
    int8_t *dst);

#ifdef __cplusplus
}
#endif
