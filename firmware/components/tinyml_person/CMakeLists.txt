idf_component_register(
    SRCS
        "person_detect_model_data.cc"
    INCLUDE_DIRS "."
    REQUIRES esp-tflite-micro esp32-camera
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
