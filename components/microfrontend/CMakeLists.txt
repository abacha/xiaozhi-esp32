# Vendored TFLite-Micro legacy audio microfrontend (the exact lib pymicro-features uses,
# which generated the Oi Enzo training features). Sourced from rhasspy/pymicro-features.
# esp-tflite-micro >=1.3 dropped this lib in favour of signal-op preprocessor models; we
# vendor it to guarantee byte-parity with training (see specs 06/07).
idf_component_register(
    SRCS
        "tensorflow/lite/experimental/microfrontend/lib/fft.cc"
        "tensorflow/lite/experimental/microfrontend/lib/fft_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/filterbank.cc"
        "tensorflow/lite/experimental/microfrontend/lib/filterbank_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/frontend.cc"
        "tensorflow/lite/experimental/microfrontend/lib/frontend_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/kiss_fft_int16.cc"
        "tensorflow/lite/experimental/microfrontend/lib/log_lut.cc"
        "tensorflow/lite/experimental/microfrontend/lib/log_scale.cc"
        "tensorflow/lite/experimental/microfrontend/lib/log_scale_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/noise_reduction.cc"
        "tensorflow/lite/experimental/microfrontend/lib/noise_reduction_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control.cc"
        "tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control_util.cc"
        "tensorflow/lite/experimental/microfrontend/lib/window.cc"
        "tensorflow/lite/experimental/microfrontend/lib/window_util.cc"
        "kissfft/kiss_fft.cc"
        "kissfft/tools/kiss_fftr.cc"
    INCLUDE_DIRS "." "kissfft"
)

# kissfft must be built fixed-point 16-bit (matches pymicro-features build).
target_compile_definitions(${COMPONENT_LIB} PRIVATE FIXED_POINT=16)
# Vendored upstream code — don't fail the build on its warnings.
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
