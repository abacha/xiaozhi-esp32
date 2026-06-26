#ifndef MICRO_WAKE_WORD_H
#define MICRO_WAKE_WORD_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <deque>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"

#include "audio_codec.h"
#include "wake_word.h"

class MicroWakeWord : public WakeWord {
public:
    MicroWakeWord();
    ~MicroWakeWord();

    bool Initialize(AudioCodec* codec, srmodel_list_t* models_list) override;
    void Feed(const std::vector<int16_t>& data) override;
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void Start() override;
    void Stop() override;
    size_t GetFeedSize() override;
    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override { return last_detected_wake_word_; }

private:
    static constexpr int kFeatureChannels = 40;
    static constexpr int kFramesPerInvoke = 3;        // model input [1,3,40], stride 3
    // Detection: the streaming model spikes high (>0.9) only briefly during the word, so we
    // count strong frames rather than averaging (which dilutes a 1-2 frame spike).
    static constexpr int kFrameWindow = 4;            // look at the last N model outputs
    static constexpr int kFrameHitsNeeded = 2;        // fire if >= this many are strong
    static constexpr int kRefractoryMs = 1000;
    static constexpr int kBootGuardMs = 8000;         // suppress only during first N ms of UPTIME (boot codec transient), not every re-arm

    AudioCodec* codec_ = nullptr;
    EventGroupHandle_t event_group_;
    std::function<void(const std::string&)> wake_word_detected_callback_;
    std::string last_detected_wake_word_ = "Oi Enzo";

    // microfrontend
    FrontendState frontend_state_ = {};
    bool frontend_ready_ = false;
    std::vector<int16_t> sample_buffer_;              // raw samples awaiting frontend

    // tflm
    tflite::MicroInterpreter* interpreter_ = nullptr;
    TfLiteTensor* input_ = nullptr;
    TfLiteTensor* output_ = nullptr;
    uint8_t* tensor_arena_ = nullptr;
    std::vector<int8_t> frame_buffer_;                // accumulates kFramesPerInvoke*40 int8
    std::deque<float> prob_window_;
    int64_t last_detection_us_ = 0;

    // feed/detection plumbing
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;
    std::condition_variable input_cv_;

    // opus capture (mirrors AfeWakeWord)
    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t* wake_word_encode_task_buffer_ = nullptr;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    std::deque<std::vector<int16_t>> wake_word_pcm_;
    std::deque<std::vector<uint8_t>> wake_word_opus_;
    std::mutex wake_word_mutex_;
    std::condition_variable wake_word_cv_;

    void DetectionTask();
    void ProcessFrame(const uint16_t* features);      // quantize + invoke + smoothing
    void StoreWakeWordData(const int16_t* data, size_t samples);
};

#endif
