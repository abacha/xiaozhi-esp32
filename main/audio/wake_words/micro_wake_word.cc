#include "micro_wake_word.h"
#include "oi_enzo_model.h"
#include "audio_service.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cmath>

#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"

#define DETECTION_RUNNING_EVENT 1
#define TAG "MicroWakeWord"

// Verified constants (spec 07). microfrontend uint16 -> float (x0.0390625) -> int8 (model scale).
static constexpr float kInScale = 0.10196078568696976f;
static constexpr int   kInZeroPoint = -128;
static constexpr float kOutScale = 1.0f / 256.0f;
static constexpr float kFeatureScale = 0.0390625f;
static constexpr int   kArenaSize = 40 * 1024;       // internal RAM; bump if AllocateTensors fails
static constexpr int   kMaxResourceVariables = 20;   // streaming-state vars (VAR_HANDLE); over-provisioned

MicroWakeWord::MicroWakeWord() {
    event_group_ = xEventGroupCreate();
}

MicroWakeWord::~MicroWakeWord() {
    if (interpreter_) { delete interpreter_; }
    if (tensor_arena_) { heap_caps_free(tensor_arena_); }
    if (frontend_ready_) { FrontendFreeStateContents(&frontend_state_); }
    if (wake_word_encode_task_stack_) heap_caps_free(wake_word_encode_task_stack_);
    if (wake_word_encode_task_buffer_) heap_caps_free(wake_word_encode_task_buffer_);
    vEventGroupDelete(event_group_);
}

bool MicroWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* /*models_list*/) {
    codec_ = codec;

    // microfrontend config — EXACT pymicro-features init_cfg values (spec 07).
    FrontendConfig cfg;
    FrontendFillConfigWithDefaults(&cfg);
    cfg.window.size_ms = 30;
    cfg.window.step_size_ms = 10;
    cfg.filterbank.num_channels = kFeatureChannels;
    cfg.filterbank.lower_band_limit = 125.0f;
    cfg.filterbank.upper_band_limit = 7500.0f;
    cfg.noise_reduction.smoothing_bits = 10;
    cfg.noise_reduction.even_smoothing = 0.025f;
    cfg.noise_reduction.odd_smoothing = 0.06f;
    cfg.noise_reduction.min_signal_remaining = 0.05f;
    cfg.pcan_gain_control.enable_pcan = 1;
    cfg.pcan_gain_control.strength = 0.95f;
    cfg.pcan_gain_control.offset = 80.0f;
    cfg.pcan_gain_control.gain_bits = 21;
    cfg.log_scale.enable_log = 1;
    cfg.log_scale.scale_shift = 6;
    if (!FrontendPopulateState(&cfg, &frontend_state_, 16000)) {
        ESP_LOGE(TAG, "FrontendPopulateState failed");
        return false;
    }
    frontend_ready_ = true;

    // TFLM interpreter
    tensor_arena_ = (uint8_t*)heap_caps_malloc(kArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!tensor_arena_) { ESP_LOGE(TAG, "tensor arena alloc failed"); return false; }

    const tflite::Model* model = tflite::GetModel(g_oi_enzo_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "model schema %lu != supported %d", (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }
    static tflite::MicroMutableOpResolver<13> resolver;
    resolver.AddVarHandle();
    resolver.AddCallOnce();
    resolver.AddReadVariable();
    resolver.AddAssignVariable();
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddConcatenation();
    resolver.AddLogistic();
    resolver.AddQuantize();
    resolver.AddReshape();
    resolver.AddSplitV();
    resolver.AddStridedSlice();

    // The streaming model keeps internal state via resource variables (VAR_HANDLE/
    // READ_VARIABLE/ASSIGN_VARIABLE), which require a MicroResourceVariables instance.
    tflite::MicroAllocator* allocator = tflite::MicroAllocator::Create(tensor_arena_, kArenaSize);
    tflite::MicroResourceVariables* resource_variables =
        tflite::MicroResourceVariables::Create(allocator, kMaxResourceVariables);
    interpreter_ = new tflite::MicroInterpreter(model, resolver, allocator, resource_variables);
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed (raise kArenaSize / try PSRAM — esphome#7242)");
        return false;
    }
    input_ = interpreter_->input(0);
    output_ = interpreter_->output(0);
    frame_buffer_.reserve(kFramesPerInvoke * kFeatureChannels);

    xTaskCreate([](void* arg){ ((MicroWakeWord*)arg)->DetectionTask(); vTaskDelete(NULL); },
                "mww_detect", 4096, this, 3, nullptr);
    ESP_LOGI(TAG, "MicroWakeWord initialized (Oi Enzo, threshold=%d%%)", CONFIG_MICRO_WAKE_WORD_THRESHOLD);
    return true;
}

void MicroWakeWord::OnWakeWordDetected(std::function<void(const std::string&)> callback) {
    wake_word_detected_callback_ = callback;
}

void MicroWakeWord::Start() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void MicroWakeWord::Stop() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    input_buffer_.clear();
    sample_buffer_.clear();
    frame_buffer_.clear();
    prob_window_.clear();
    if (frontend_ready_) FrontendReset(&frontend_state_);
}

size_t MicroWakeWord::GetFeedSize() {
    return 160;   // 10ms @ 16kHz; audio_service feeds 160-sample chunks
}

void MicroWakeWord::Feed(const std::vector<int16_t>& data) {
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    if (!(xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT)) return;
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());
    input_cv_.notify_one();
}

void MicroWakeWord::DetectionTask() {
    ESP_LOGI(TAG, "MicroWakeWord detection task started");
    while (true) {
        std::vector<int16_t> chunk;
        {
            std::unique_lock<std::mutex> lock(input_buffer_mutex_);
            input_cv_.wait(lock, [this]{ return !input_buffer_.empty(); });
            chunk.swap(input_buffer_);
        }
        if (!(xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT)) continue;

        StoreWakeWordData(chunk.data(), chunk.size());            // raw pcm for opus handoff

        // [mic] level meter (diagnostic): how hot is the raw mic feed?
        {
            int peak = 0; double sumsq = 0;
            for (int16_t s : chunk) { int a = s < 0 ? -s : s; if (a > peak) peak = a; sumsq += (double)s * s; }
            float rms = chunk.empty() ? 0 : sqrtf(sumsq / chunk.size());
            static int lvln = 0;
            if (++lvln % 25 == 0 || peak > 2000)
                ESP_LOGD(TAG, "[mic] peak=%d (%.1f dBFS) rms=%.0f", peak, 20.0f * log10f((peak + 1) / 32768.0f), rms);
        }

        sample_buffer_.insert(sample_buffer_.end(), chunk.begin(), chunk.end());

        size_t idx = 0;
        while (idx < sample_buffer_.size()) {
            size_t num_read = 0;
            FrontendOutput out = FrontendProcessSamples(
                &frontend_state_, sample_buffer_.data() + idx,
                sample_buffer_.size() - idx, &num_read);
            idx += num_read;
            if (num_read == 0) break;                              // need more samples
            if (out.size == (size_t)kFeatureChannels) ProcessFrame(out.values);
        }
        if (idx > 0) sample_buffer_.erase(sample_buffer_.begin(), sample_buffer_.begin() + idx);
    }
}

void MicroWakeWord::ProcessFrame(const uint16_t* features) {
    for (int c = 0; c < kFeatureChannels; c++) {
        float f = features[c] * kFeatureScale;
        long q = lroundf(f / kInScale) + kInZeroPoint;
        if (q < -128) q = -128; else if (q > 127) q = 127;
        frame_buffer_.push_back((int8_t)q);
    }
    if ((int)frame_buffer_.size() < kFramesPerInvoke * kFeatureChannels) return;

    std::memcpy(input_->data.int8, frame_buffer_.data(), frame_buffer_.size());
    frame_buffer_.clear();
    if (interpreter_->Invoke() != kTfLiteOk) { ESP_LOGW(TAG, "invoke failed"); return; }
    float prob = output_->data.uint8[0] * kOutScale;

    const float frame_thr = CONFIG_MICRO_WAKE_WORD_THRESHOLD / 100.0f;   // per-frame strong-prob threshold (Kconfig)
    prob_window_.push_back(prob);
    if ((int)prob_window_.size() > kFrameWindow) prob_window_.pop_front();
    int hits = 0;
    for (float p : prob_window_) if (p >= frame_thr) hits++;

    if (prob >= 0.3f) ESP_LOGD(TAG, "[probe] frame=%.2f hits=%d/%d", prob, hits, (int)prob_window_.size());

    int64_t now = esp_timer_get_time();
    if (now < (int64_t)kBootGuardMs * 1000) return;   // boot-only guard (uptime), not per re-arm
    if (hits >= kFrameHitsNeeded &&
        (now - last_detection_us_) > (int64_t)kRefractoryMs * 1000) {
        last_detection_us_ = now;
        ESP_LOGI(TAG, "wake detected (%d/%d frames >= %.2f)", hits, (int)prob_window_.size(), frame_thr);
        Stop();
        if (wake_word_detected_callback_) wake_word_detected_callback_(last_detected_wake_word_);
    }
}

void MicroWakeWord::StoreWakeWordData(const int16_t* data, size_t samples) {
    wake_word_pcm_.emplace_back(std::vector<int16_t>(data, data + samples));
    // keep ~2 seconds; chunks vary, so cap by total samples (~2s * 16000)
    size_t total = 0;
    for (auto& v : wake_word_pcm_) total += v.size();
    while (total > 2 * 16000 && wake_word_pcm_.size() > 1) {
        total -= wake_word_pcm_.front().size();
        wake_word_pcm_.pop_front();
    }
}

void MicroWakeWord::EncodeWakeWordData() {
    const size_t stack_size = 4096 * 6;
    wake_word_opus_.clear();
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM);
        assert(wake_word_encode_task_stack_ != nullptr);
    }
    if (wake_word_encode_task_buffer_ == nullptr) {
        wake_word_encode_task_buffer_ = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
        assert(wake_word_encode_task_buffer_ != nullptr);
    }

    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (MicroWakeWord*)arg;
        {
            auto start_time = esp_timer_get_time();
            esp_opus_enc_config_t opus_enc_cfg = AS_OPUS_ENC_CONFIG();
            void* encoder_handle = nullptr;
            auto ret = esp_opus_enc_open(&opus_enc_cfg, sizeof(esp_opus_enc_config_t), &encoder_handle);
            if (encoder_handle == nullptr) {
                ESP_LOGE(TAG, "Failed to create audio encoder, error code: %d", ret);
                std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                this_->wake_word_opus_.push_back(std::vector<uint8_t>());
                this_->wake_word_cv_.notify_all();
                return;
            }

            int frame_size = 0;
            int outbuf_size = 0;
            esp_opus_enc_get_frame_size(encoder_handle, &frame_size, &outbuf_size);
            frame_size = frame_size / sizeof(int16_t);

            int packets = 0;
            std::vector<int16_t> in_buffer;
            esp_audio_enc_in_frame_t in = {};
            esp_audio_enc_out_frame_t out = {};

            for (auto& pcm: this_->wake_word_pcm_) {
                if (in_buffer.empty()) {
                    in_buffer = std::move(pcm);
                } else {
                    in_buffer.reserve(in_buffer.size() + pcm.size());
                    in_buffer.insert(in_buffer.end(), pcm.begin(), pcm.end());
                }

                while (in_buffer.size() >= (size_t)frame_size) {
                    std::vector<uint8_t> opus_buf(outbuf_size);
                    in.buffer = (uint8_t *)(in_buffer.data());
                    in.len = (uint32_t)(frame_size * sizeof(int16_t));
                    out.buffer = opus_buf.data();
                    out.len = outbuf_size;
                    out.encoded_bytes = 0;

                    ret = esp_opus_enc_process(encoder_handle, &in, &out);
                    if (ret == ESP_AUDIO_ERR_OK) {
                        std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                        this_->wake_word_opus_.emplace_back(opus_buf.data(), opus_buf.data() + out.encoded_bytes);
                        this_->wake_word_cv_.notify_all();
                        packets++;
                    } else {
                        ESP_LOGE(TAG, "Failed to encode audio, error code: %d", ret);
                    }
                    in_buffer.erase(in_buffer.begin(), in_buffer.begin() + frame_size);
                }
            }
            this_->wake_word_pcm_.clear();
            esp_opus_enc_close(encoder_handle);
            auto end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "Encode wake word opus %d packets in %ld ms", packets, (long)((end_time - start_time) / 1000));

            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
            this_->wake_word_opus_.push_back(std::vector<uint8_t>());
            this_->wake_word_cv_.notify_all();
        }
        vTaskDelete(NULL);
    }, "encode_wake_word", stack_size, this, 2, wake_word_encode_task_stack_, wake_word_encode_task_buffer_);
}

bool MicroWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty();
    });
    opus.swap(wake_word_opus_.front());
    wake_word_opus_.pop_front();
    return !opus.empty();
}
