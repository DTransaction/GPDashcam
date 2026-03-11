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

void rgb565_to_rgb96(const uint16_t *src,
                            int src_w,
                            int src_h,
                            int8_t *dst)
{

    const int x_ratio = (src_w << 16) / ML_W;
    const int y_ratio = (src_h << 16) / ML_H;

    int y_acc = 0;

    for (int y = 0; y < ML_H; y++) {

        int sy = (y_acc >> 16) * src_w;
        int x_acc = 0;

        for (int x = 0; x < ML_W; x++) {

            uint16_t p = src[sy + (x_acc >> 16)];

            uint8_t r = (p >> 11) & 0x1F;
            uint8_t g = (p >> 5)  & 0x3F;
            uint8_t b =  p        & 0x1F;

            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);

            int dst_index = (y * ML_W + x) * 3;

            dst[dst_index + 0] = r - 128;
            dst[dst_index + 1] = g - 128;
            dst[dst_index + 2] = b - 128;

            x_acc += x_ratio;
        }

        y_acc += y_ratio;
    }
}

void rgb565_to_rgb96_char(
    const unsigned char *src,
    int src_w,
    int src_h,
    int8_t *dst)
{
    const uint16_t *src16 = (const uint16_t*)src;

    const int x_ratio = (src_w << 16) / ML_W;
    const int y_ratio = (src_h << 16) / ML_H;

    int y_acc = 0;

    for (int y = 0; y < ML_H; y++) {

        int sy = (y_acc >> 16) * src_w;
        int x_acc = 0;

        for (int x = 0; x < ML_W; x++) {

            uint16_t p = src16[sy + (x_acc >> 16)];

            uint8_t r = (p >> 11) & 0x1F;
            uint8_t g = (p >> 5)  & 0x3F;
            uint8_t b =  p        & 0x1F;

            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);

            int dst_index = (y * ML_W + x) * 3;

            dst[dst_index + 0] = r - 128;
            dst[dst_index + 1] = g - 128;
            dst[dst_index + 2] = b - 128;

            x_acc += x_ratio;
        }

        y_acc += y_ratio;
    }
}

void draw_status_bar_rgb565(camera_fb_t* fb, uint16_t color)
{
    uint16_t* img = (uint16_t*)fb->buf;

    int bar_height = 10; // pixels

    for (int y = 0; y < bar_height && y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            img[y * fb->width + x] = color;
        }
    }
}

void preprocess_image(
    uint8_t* buf,
    int width,
    int height,
    int8_t* input_data)
{
    for (int i = 0; i < width * height * 2; i++) {
        // input_data[i] = ((uint8_t *) buf)[i] ^ 0x80;
        // input_data[i] = ((uint8_t *) buf)[i] - 128;

        input_data[i] = ((uint8_t *) buf)[i];
    }

    // debugging model
    // for (int i = 0; i < width * height * 3; i++){
    //     input->data.int8[i] = 0;
    // }
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

    // printf("first pixel: %d %d %d\n",
    //    input->data.int8[0],
    //    input->data.int8[1],
    //    input->data.int8[2]);

    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Invoke failed\n");
        return false;
        }

    // printf("scale: %f\n", input->params.scale);
    // printf("zero: %ld\n", input->params.zero_point);

    // for (int i = 0; i < 12; i++)
    //     printf("%d ", input->data.int8[i]);

    // printf("Input bytes: %d\n", input->bytes);
    // printf("Input type: %d\n", input->type);

    int8_t raw_score = output->data.int8[0];

    bool stop_detected = raw_score > 0;

    if(stop_detected) {
        printf("Stop detected, score: %d\n", raw_score);
    }
    else {
        printf("Stop not detected, score: %d\n", raw_score);
    }

    return stop_detected;

    // int8_t person_score = output->data.int8[1];
    // int8_t no_person_score = output->data.int8[0];

    // float person_score_f =
    //   (person_score - output->params.zero_point) * output->params.scale;
    // float no_person_score_f =
    //   (no_person_score - output->params.zero_point) * output->params.scale;

    // //DEBUG        
    // bool person_detected = person_score_f > no_person_score_f;
    // if(person_detected) {
    //     printf("Person detected: Output float: %.3f %.3f\n",
    //     no_person_score_f,
    //     person_score_f);
    // }
    // else {
    //     printf("No person detected: Output float: %.3f %.3f\n",
    //     no_person_score_f,
    //     person_score_f);
    // }

    // return person_score_f;
}

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

void rgb888_to_rgb96(
    const unsigned char *src,
    int src_w,
    int src_h,
    int8_t *dst)
{
    const int x_ratio = (src_w << 16) / ML_W;
    const int y_ratio = (src_h << 16) / ML_H;

    int y_acc = 0;

    for (int y = 0; y < ML_H; y++) {

        int sy = (y_acc >> 16) * src_w;
        int x_acc = 0;

        for (int x = 0; x < ML_W; x++) {

            int src_index = (sy + (x_acc >> 16)) * 3;

            uint8_t r = src[src_index + 0];
            uint8_t g = src[src_index + 1];
            uint8_t b = src[src_index + 2];

            int dst_index = (y * ML_W + x) * 3;

            dst[dst_index + 0] = r - 128;
            dst[dst_index + 1] = g - 128;
            dst[dst_index + 2] = b - 128;

            x_acc += x_ratio;
        }

        y_acc += y_ratio;
    }
}

void dump_tensor(int8_t *tensor)
{
    for (int i = 0; i < ML_W * ML_H * 3; i++) {
        printf("%d", tensor[i]);
        printf(",");
    }
    printf("\n");
}

void rgb565_to_rgb888(
    const uint16_t *src,
    int width,
    int height,
    int8_t *dst)
{
    int num_pixels = width * height;

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