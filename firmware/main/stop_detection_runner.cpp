extern "C" {
#include "esp_camera.h"
#include "stop_detection_runner.h"
#include "esp_heap_caps.h"

// debug : test image from dataset
// #include "test_image.h"
}

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "stop_detect_model_data.h"
#include "model_settings.h"

#define DST_W 96
#define DST_H 96

#define TENSOR_ARENA_SIZE (100 * 1024)
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];
// uint8_t *tensor_arena = NULL;

static tflite::MicroInterpreter* interpreter;
static TfLiteTensor* input;
static TfLiteTensor* output;

void tinyml_init(void)
{
    // tensor_arena = (uint8_t*) heap_caps_malloc(TENSOR_ARENA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
   
    // if (!tensor_arena) {
    //     printf("Failed to allocate tensor arena!\n");
    //     return;
    // }

    const tflite::Model* model = tflite::GetModel(g_stop_detect_model_data);
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

    printf("Arena used: %d bytes\n", interpreter->arena_used_bytes());

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

int8_t run_stop_detection(const uint8_t *src_bytes, int src_width, int src_height) {
    resize_and_rgb565_to_rgb888(src_bytes, src_width, src_height, input->data.int8);

    if (interpreter->Invoke() != kTfLiteOk) {
        printf("Invoke failed\n");
        return -127;
	}

    return (int8_t)output->data.int8[0];
}

void resize_and_rgb565_to_rgb888(
    const uint8_t *src_bytes,
    int src_width,
    int src_height,
    int8_t *dst)
{

    float x_ratio = (float)(src_width - 1) / DST_W;
    float y_ratio = (float)(src_height - 1) / DST_H;

    int counter = 0;

    for (int j = 0; j < DST_H; j++)
    {
        for (int i = 0; i < DST_W; i++)
        {
            float gx = i * x_ratio;
            float gy = j * y_ratio;

            int x = (int)gx;
            int y = (int)gy;

            float dx = gx - x;
            float dy = gy - y;

            int idx00 = y * src_width + x;
            int idx10 = y * src_width + (x + 1);
            int idx01 = (y + 1) * src_width + x;
            int idx11 = (y + 1) * src_width + (x + 1);

            // swap to account for endianess of camera output
            uint16_t p00 = (src_bytes[idx00*2] << 8) | src_bytes[idx00*2 + 1];
            uint16_t p10 = (src_bytes[idx10*2] << 8) | src_bytes[idx10*2 + 1];
            uint16_t p01 = (src_bytes[idx01*2] << 8) | src_bytes[idx01*2 + 1];
            uint16_t p11 = (src_bytes[idx11*2] << 8) | src_bytes[idx11*2 + 1];

            // extract RGB
            int r00 = (p00 >> 11) & 0x1F;
            int g00 = (p00 >> 5) & 0x3F;
            int b00 = p00 & 0x1F;

            int r10 = (p10 >> 11) & 0x1F;
            int g10 = (p10 >> 5) & 0x3F;
            int b10 = p10 & 0x1F;

            int r01 = (p01 >> 11) & 0x1F;
            int g01 = (p01 >> 5) & 0x3F;
            int b01 = p01 & 0x1F;

            int r11 = (p11 >> 11) & 0x1F;
            int g11 = (p11 >> 5) & 0x3F;
            int b11 = p11 & 0x1F;

            // bilinear interpolation
            int r = r00*(1-dx)*(1-dy) + r10*(dx)*(1-dy) + r01*(1-dx)*dy + r11*dx*dy;
            int g = g00*(1-dx)*(1-dy) + g10*(dx)*(1-dy) + g01*(1-dx)*dy + g11*dx*dy;
            int b = b00*(1-dx)*(1-dy) + b10*(dx)*(1-dy) + b01*(1-dx)*dy + b11*dx*dy;

            // convert from RGB565 to RGB888
            r = (r * 527 + 23) >> 6;
            g = (g * 259 + 33) >> 6;
            b = (b * 527 + 23) >> 6;

            dst[counter*3 + 0] = (int8_t)(r - 128);
            dst[counter*3 + 1] = (int8_t)(g - 128);
            dst[counter*3 + 2] = (int8_t)(b - 128);

            counter++;
        }
    }
}
