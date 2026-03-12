extern "C" {
#include "esp_camera.h"
#include "person_detection_runner.h"

// debug : test image from dataset
// #include "test_image.h"
}

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "person_detect_model_data.h"
#include "model_settings.h"

// #include <opencv2/opencv.hpp>

#define ML_W 96
#define ML_H 96
#define CHANNELS 3

#define TENSOR_ARENA_SIZE (200 * 1024)
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

static tflite::MicroInterpreter* interpreter;
static TfLiteTensor* input;
static TfLiteTensor* output;

void tinyml_init(void)
{
    const tflite::Model* model = tflite::GetModel(g_person_detect_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Model schema mismatch!\n");
        return;
    }

    static tflite::MicroMutableOpResolver<11> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddAveragePool2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddFullyConnected();
    resolver.AddShape();
    resolver.AddStridedSlice();
    resolver.AddPack();
    resolver.AddLogistic();


    static tflite::MicroInterpreter static_interpreter(
        model,
        resolver,
        tensor_arena,
        TENSOR_ARENA_SIZE
    );

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("AllocateTensors failed\n");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    printf("Input dims: ");

    for(int i = 0; i < input->dims->size; i++)
    {
        printf("%d ", input->dims->data[i]);
    }

    printf("Input bytes: %d\n", input->bytes);

    printf("Input type: %d\n", input->type);

    printf("Output dims: ");

    for(int i = 0; i < output->dims->size; i++)
    {
        printf("%d ", output->dims->data[i]);
    }

    printf("\n");

    printf("\n");
    printf("TinyML initialized\n");
}

bool run_stop_detection(const int8_t* ml_input)
{

    // rgb888_to_rgb96_int8(
    // no_test_image_rgb,
    // 96,
    // 96,
    // ml_input);
    
    // preprocess_image(
    //     (uint8_t*)ml_gray,
    //     ML_W,
    //     ML_H,
    //     input->data.int8
    // );

    // dump_tensor(input->data.int8);

    memcpy(input->data.int8, ml_input, 96 * 96 * 3);

    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Invoke failed\n");
        return false;
        }

    int8_t raw_score = output->data.int8[0];

    bool stop_detected = raw_score > 0;

    if(stop_detected) {
        printf("Stop detected, score: %d\n", raw_score);
    }
    else {
        printf("Stop not detected, score: %d\n", raw_score);
    }

    return stop_detected;
}

// void rgb565_to_gray96(
//     const uint16_t *src,
//     int src_w,
//     int src_h,
//     uint8_t *dst)
// {
//     const int x_ratio = (src_w << 16) / ML_W;
//     const int y_ratio = (src_h << 16) / ML_H;

//     int y_acc = 0;

//     for (int y = 0; y < ML_H; y++) {
//         int sy = (y_acc >> 16) * src_w;
//         int x_acc = 0;

//         for (int x = 0; x < ML_W; x++) {
//             uint16_t p = src[sy + (x_acc >> 16)];

//             // fast grayscale from RGB565
//             uint8_t r = (p >> 11) & 0x1F;
//             uint8_t g = (p >> 5)  & 0x3F;
//             uint8_t b =  p        & 0x1F;

//             // dst[y * ML_W + x] = (r * 76 + g * 150 + b * 30) >> 8;
//             dst[(ML_H - 1 - y) * ML_W + x] =
//                 (r * 76 + g * 150 + b * 30) >> 8;
        
//             x_acc += x_ratio;
//         }

//         y_acc += y_ratio;
//     }
// }

// void rgb565_to_rgb96(const uint16_t *src,
//                             int src_w,
//                             int src_h,
//                             int8_t *dst)
// {

//     const int x_ratio = (src_w << 16) / ML_W;
//     const int y_ratio = (src_h << 16) / ML_H;

//     int y_acc = 0;

//     for (int y = 0; y < ML_H; y++) {

//         int sy = (y_acc >> 16) * src_w;
//         int x_acc = 0;

//         for (int x = 0; x < ML_W; x++) {

//             uint16_t p = src[sy + (x_acc >> 16)];

//             uint8_t r = (p >> 11) & 0x1F;
//             uint8_t g = (p >> 5)  & 0x3F;
//             uint8_t b =  p        & 0x1F;

//             r = (r << 3) | (r >> 2);
//             g = (g << 2) | (g >> 4);
//             b = (b << 3) | (b >> 2);

//             int dst_index = (y * ML_W + x) * 3;

//             dst[dst_index + 0] = r - 128;
//             dst[dst_index + 1] = g - 128;
//             dst[dst_index + 2] = b - 128;

//             x_acc += x_ratio;
//         }

//         y_acc += y_ratio;
//     }
// }

// void rgb565_to_rgb96_char(
//     const unsigned char *src,
//     int src_w,
//     int src_h,
//     int8_t *dst)
// {
//     const uint16_t *src16 = (const uint16_t*)src;

//     const int x_ratio = (src_w << 16) / ML_W;
//     const int y_ratio = (src_h << 16) / ML_H;

//     int y_acc = 0;

//     for (int y = 0; y < ML_H; y++) {

//         int sy = (y_acc >> 16) * src_w;
//         int x_acc = 0;

//         for (int x = 0; x < ML_W; x++) {

//             uint16_t p = src16[sy + (x_acc >> 16)];

//             uint8_t r = (p >> 11) & 0x1F;
//             uint8_t g = (p >> 5)  & 0x3F;
//             uint8_t b =  p        & 0x1F;

//             r = (r << 3) | (r >> 2);
//             g = (g << 2) | (g >> 4);
//             b = (b << 3) | (b >> 2);

//             int dst_index = (y * ML_W + x) * 3;

//             dst[dst_index + 0] = r - 128;
//             dst[dst_index + 1] = g - 128;
//             dst[dst_index + 2] = b - 128;

//             x_acc += x_ratio;
//         }

//         y_acc += y_ratio;
//     }
// }

// void draw_status_bar_rgb565(camera_fb_t* fb, uint16_t color)
// {
//     uint16_t* img = (uint16_t*)fb->buf;

//     int bar_height = 10; // pixels

//     for (int y = 0; y < bar_height && y < fb->height; y++) {
//         for (int x = 0; x < fb->width; x++) {
//             img[y * fb->width + x] = color;
//         }
//     }
// }

// void preprocess_image(
//     uint8_t* buf,
//     int width,
//     int height,
//     int8_t* input_data)
// {
//     for (int i = 0; i < width * height * 2; i++) {
//         // input_data[i] = ((uint8_t *) buf)[i] ^ 0x80;
//         // input_data[i] = ((uint8_t *) buf)[i] - 128;

//         input_data[i] = ((uint8_t *) buf)[i];
//     }

//     // debugging model
//     // for (int i = 0; i < width * height * 3; i++){
//     //     input->data.int8[i] = 0;
//     // }
// }



// void run_test_image(void)
// {
//     for(int i = 0; i < 96*96*3; i++)
//     {
//         input->data.int8[i] = test_image_rgb[i] - 128;
//     }

//     if (interpreter->Invoke() != kTfLiteOk) {
//         printf("Invoke failed\n");
//         return;
//     }

//     int8_t no_stop = output->data.int8[0];
//     int8_t stop    = output->data.int8[1];

//     float no_stop_f =
//         (no_stop - output->params.zero_point) * output->params.scale;

//     float stop_f =
//         (stop - output->params.zero_point) * output->params.scale;

//     printf("NO_STOP: %.3f  STOP: %.3f\n", no_stop_f, stop_f);
// }

// void rgb888_to_rgb96(
//     const unsigned char *src,
//     int src_w,
//     int src_h,
//     int8_t *dst)
// {
//     const int x_ratio = (src_w << 16) / ML_W;
//     const int y_ratio = (src_h << 16) / ML_H;

//     int y_acc = 0;

//     for (int y = 0; y < ML_H; y++) {

//         int sy = (y_acc >> 16) * src_w;
//         int x_acc = 0;

//         for (int x = 0; x < ML_W; x++) {

//             int src_index = (sy + (x_acc >> 16)) * 3;

//             uint8_t r = src[src_index + 0];
//             uint8_t g = src[src_index + 1];
//             uint8_t b = src[src_index + 2];

//             int dst_index = (y * ML_W + x) * 3;

//             dst[dst_index + 0] = r - 128;
//             dst[dst_index + 1] = g - 128;
//             dst[dst_index + 2] = b - 128;

//             x_acc += x_ratio;
//         }

//         y_acc += y_ratio;
//     }
// }

// void dump_tensor(int8_t *tensor)
// {
//     for (int i = 0; i < ML_W * ML_H * 3; i++) {
//         printf("%d", tensor[i]);
//         printf(",");
//     }
//     printf("\n");
// }

void resize_and_rgb565_to_rgb888(
    const uint16_t *src,
    int src_width,
    int src_height,
    int8_t *dst)
{
    // int8_t resized_dst[ML_W * ML_H * CHANNELS];
    // for (int y = 0; y < ML_H; y++) {
    //     for (int x = 0; x < ML_W; x++) {
    //         // Find corresponding pixel in source
    //         int srcX = (x * src_width) / ML_W;
    //         int srcY = (y * src_height) / ML_H;
    //         for (int c = 0; c < CHANNELS; c++) {
    //             resized_dst[(y * ML_W + x) * CHANNELS + c] = 
    //                 src[(srcY * src_width + srcX) * CHANNELS + c];
    //         }
    //     }
    // }

    int num_pixels = ML_W * ML_H;

    for (int i = 0; i < num_pixels; i++) {

        uint16_t p = src[i];

        uint8_t r = (p >> 11) & 0x1F;
        uint8_t g = (p >> 5)  & 0x3F;
        uint8_t b =  p        & 0x1F;

        // r = (r << 3) | (r >> 2);
        // g = (g << 2) | (g >> 4);
        // b = (b << 3) | (b >> 2);
        r = (r * 527 + 23) >> 6; 
        g = (g * 259 + 33) >> 6; 
        b = (b * 527 + 23) >> 6; 

        dst[i*3 + 0] = (int8_t)((int)r - 128);
        dst[i*3 + 1] = (int8_t)((int)g - 128);
        dst[i*3 + 2] = (int8_t)((int)b - 128);
    }
}

void resize_and_rgb565_to_rgb888_2(
    const uint8_t *src,
    int src_width,
    int src_height,
    int8_t *dst)
{

        // int8_t resized_dst[ML_W * ML_H * CHANNELS];
    // for (int y = 0; y < ML_H; y++) {
    //     for (int x = 0; x < ML_W; x++) {
    //         // Find corresponding pixel in source
    //         int srcX = (x * src_width) / ML_W;
    //         int srcY = (y * src_height) / ML_H;
    //         for (int c = 0; c < CHANNELS; c++) {
    //             resized_dst[(y * ML_W + x) * CHANNELS + c] = 
    //                 src[(srcY * src_width + srcX) * CHANNELS + c];
    //         }
    //     }
    // }

    int num_pixels = ML_W * ML_H;

    for (int i = 0; i < num_pixels; i++)
    {
        uint8_t lo = src[i*2 + 0];
        uint8_t hi = src[i*2 + 1];

        uint16_t p = (lo << 8) | hi;   // correct ordering

        uint8_t r = (p >> 11) & 0x1F;
        uint8_t g = (p >> 5)  & 0x3F;
        uint8_t b =  p        & 0x1F;

        // r = (r << 3) | (r >> 2);
        // g = (g << 2) | (g >> 4);
        // b = (b << 3) | (b >> 2);

        r = (r * 527 + 23) >> 6;
        g = (g * 259 + 33) >> 6;
        b = (b * 527 + 23) >> 6;

        dst[i*3 + 0] = (int8_t)(r - 128);
        dst[i*3 + 1] = (int8_t)(g - 128);
        dst[i*3 + 2] = (int8_t)(b - 128);
    }


    // -------------------------- //

    // int num_pixels = ML_W * ML_H;

    // for (int i = 0; i < num_pixels; i++) {

    //     uint16_t p = src[i];

    //     uint8_t r = (p >> 11) & 0x1F;
    //     uint8_t g = (p >> 5)  & 0x3F;
    //     uint8_t b =  p        & 0x1F;

    //     // r = (r << 3) | (r >> 2);
    //     // g = (g << 2) | (g >> 4);
    //     // b = (b << 3) | (b >> 2);
    //     r = (r * 527 + 23) >> 6; 
    //     g = (g * 259 + 33) >> 6; 
    //     b = (b * 527 + 23) >> 6; 

    //     dst[i*3 + 0] = (int8_t)((int)r - 128);
    //     dst[i*3 + 1] = (int8_t)((int)g - 128);
    //     dst[i*3 + 2] = (int8_t)((int)b - 128);
    // }
}

// void resizeNearestNeighbor(uint16_t* src, int srcW, int srcH, 
//                           uint16_t* dst, int dstW, int dstH, int channels) 
// {
//     for (int y = 0; y < dstH; y++) {
//         for (int x = 0; x < dstW; x++) {
//             // Find corresponding pixel in source
//             int srcX = (x * srcW) / dstW;
//             int srcY = (y * srcH) / dstH;
//             for (int c = 0; c < channels; c++) {
//                 dst[(y * dstW + x) * channels + c] = 
//                     src[(srcY * srcW + srcX) * channels + c];
//             }
//         }
//     }
// }

// void rgb565_bilinear_resize_to_tensor(
//     const uint16_t *src,
//     int src_w,
//     int src_h,
//     int8_t *dst)
// {
//     const int OUT = 96;

//     float x_ratio = (float)(src_w - 1) / OUT;
//     float y_ratio = (float)(src_h - 1) / OUT;

//     for (int y = 0; y < OUT; y++) {

//         float sy = y * y_ratio;
//         int y0 = (int)sy;
//         int y1 = y0 + 1;
//         float dy = sy - y0;

//         if (y1 >= src_h) y1 = src_h - 1;

//         for (int x = 0; x < OUT; x++) {

//             float sx = x * x_ratio;
//             int x0 = (int)sx;
//             int x1 = x0 + 1;
//             float dx = sx - x0;

//             if (x1 >= src_w) x1 = src_w - 1;

//             uint16_t p00 = src[y0 * src_w + x0];
//             uint16_t p10 = src[y0 * src_w + x1];
//             uint16_t p01 = src[y1 * src_w + x0];
//             uint16_t p11 = src[y1 * src_w + x1];

//             uint8_t r00 = (p00 >> 11) & 0x1F;
//             uint8_t g00 = (p00 >> 5)  & 0x3F;
//             uint8_t b00 =  p00        & 0x1F;

//             uint8_t r10 = (p10 >> 11) & 0x1F;
//             uint8_t g10 = (p10 >> 5)  & 0x3F;
//             uint8_t b10 =  p10        & 0x1F;

//             uint8_t r01 = (p01 >> 11) & 0x1F;
//             uint8_t g01 = (p01 >> 5)  & 0x3F;
//             uint8_t b01 =  p01        & 0x1F;

//             uint8_t r11 = (p11 >> 11) & 0x1F;
//             uint8_t g11 = (p11 >> 5)  & 0x3F;
//             uint8_t b11 =  p11        & 0x1F;

//             float w00 = (1-dx)*(1-dy);
//             float w10 = dx*(1-dy);
//             float w01 = (1-dx)*dy;
//             float w11 = dx*dy;

//             float r = r00*w00 + r10*w10 + r01*w01 + r11*w11;
//             float g = g00*w00 + g10*w10 + g01*w01 + g11*w11;
//             float b = b00*w00 + b10*w10 + b01*w01 + b11*w11;

//             r = r * 255.0f / 31.0f;
//             g = g * 255.0f / 63.0f;
//             b = b * 255.0f / 31.0f;

//             int idx = (y * OUT + x) * 3;

//             dst[idx+0] = (int8_t)((int)r - 128);
//             dst[idx+1] = (int8_t)((int)g - 128);
//             dst[idx+2] = (int8_t)((int)b - 128);
//         }
//     }
// }

// #define SRC_W 640
// #define SRC_H 480

// #define CROP_SIZE 480
// #define DST_W 96
// #define DST_H 96

// void crop_resize_rgb565_bilinear_to_tensor(
//     const uint16_t *src,
//     int8_t *dst)
// {
//     const int crop_x = (SRC_W - CROP_SIZE) / 2; // 80
//     const int crop_y = 0;

//     const float x_scale = (float)(CROP_SIZE - 1) / (DST_W - 1);
//     const float y_scale = (float)(CROP_SIZE - 1) / (DST_H - 1);

//     for (int y = 0; y < DST_H; y++)
//     {
//         float sy = y * y_scale;

//         int y0 = (int)sy;
//         int y1 = y0 + 1;
//         if (y1 >= CROP_SIZE) y1 = CROP_SIZE - 1;

//         float wy = sy - y0;

//         for (int x = 0; x < DST_W; x++)
//         {
//             float sx = x * x_scale;

//             int x0 = (int)sx;
//             int x1 = x0 + 1;
//             if (x1 >= CROP_SIZE) x1 = CROP_SIZE - 1;

//             float wx = sx - x0;

//             int src_x0 = crop_x + x0;
//             int src_x1 = crop_x + x1;

//             int src_y0 = crop_y + y0;
//             int src_y1 = crop_y + y1;

//             uint16_t p00 = src[src_y0 * SRC_W + src_x0];
//             uint16_t p01 = src[src_y0 * SRC_W + src_x1];
//             uint16_t p10 = src[src_y1 * SRC_W + src_x0];
//             uint16_t p11 = src[src_y1 * SRC_W + src_x1];

//             // Extract RGB565
//             uint8_t r00 = (p00 >> 11) & 0x1F;
//             uint8_t g00 = (p00 >> 5)  & 0x3F;
//             uint8_t b00 =  p00        & 0x1F;

//             uint8_t r01 = (p01 >> 11) & 0x1F;
//             uint8_t g01 = (p01 >> 5)  & 0x3F;
//             uint8_t b01 =  p01        & 0x1F;

//             uint8_t r10 = (p10 >> 11) & 0x1F;
//             uint8_t g10 = (p10 >> 5)  & 0x3F;
//             uint8_t b10 =  p10        & 0x1F;

//             uint8_t r11 = (p11 >> 11) & 0x1F;
//             uint8_t g11 = (p11 >> 5)  & 0x3F;
//             uint8_t b11 =  p11        & 0x1F;

//             // Expand to 8-bit
//             r00 = (r00 << 3) | (r00 >> 2);
//             g00 = (g00 << 2) | (g00 >> 4);
//             b00 = (b00 << 3) | (b00 >> 2);

//             r01 = (r01 << 3) | (r01 >> 2);
//             g01 = (g01 << 2) | (g01 >> 4);
//             b01 = (b01 << 3) | (b01 >> 2);

//             r10 = (r10 << 3) | (r10 >> 2);
//             g10 = (g10 << 2) | (g10 >> 4);
//             b10 = (b10 << 3) | (b10 >> 2);

//             r11 = (r11 << 3) | (r11 >> 2);
//             g11 = (g11 << 2) | (g11 >> 4);
//             b11 = (b11 << 3) | (b11 >> 2);

//             // Bilinear interpolation
//             float w00 = (1 - wx) * (1 - wy);
//             float w01 = wx * (1 - wy);
//             float w10 = (1 - wx) * wy;
//             float w11 = wx * wy;

//             float r = r00*w00 + r01*w01 + r10*w10 + r11*w11;
//             float g = g00*w00 + g01*w01 + g10*w10 + g11*w11;
//             float b = b00*w00 + b01*w01 + b10*w10 + b11*w11;

//             int idx = (y * DST_W + x) * 3;

//             dst[idx + 0] = (int8_t)((int)r - 128);
//             dst[idx + 1] = (int8_t)((int)g - 128);
//             dst[idx + 2] = (int8_t)((int)b - 128);
//         }
//     }
// }