#include "stt_engine.h"
#include "logger.h"
#include <whisper.h>
#include <fstream>
#include <chrono>
#include <sstream>

namespace memo_rf {

class STTEngine::Impl {
public:
    Impl(const STTConfig& config) : config_(config), ctx_(nullptr), ready_(false) {
        // Load whisper model
        if (config_.model_path.empty()) {
            LOG_STT("No model path specified");
            return;
        }
        
        // Initialize whisper (Metal GPU on macOS when whisper.cpp built with GGML_METAL)
        struct whisper_context_params cparams = whisper_context_default_params();
        cparams.use_gpu = config_.use_gpu;
        
        ctx_ = whisper_init_from_file_with_params(config_.model_path.c_str(), cparams);
        if (!ctx_) {
            LOG_STT("Failed to load whisper model: " + config_.model_path);
            return;
        }
        
        ready_ = true;
        LOG_STT("Model loaded successfully");
    }
    
    ~Impl() {
        if (ctx_) {
            whisper_free(ctx_);
        }
    }
    
    Transcript transcribe(const AudioBuffer& segment) {
        Transcript result;
        result.confidence = 0.0f;
        result.processing_ms = 0;
        
        if (!ctx_ || segment.empty()) {
            return result;
        }
        
        auto start = std::chrono::steady_clock::now();
        
        // Prepare whisper parameters
        struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        params.print_progress = false;
        params.print_special = false;
        params.print_realtime = false;
        params.translate = false;
        params.language = config_.language.c_str();
        params.n_threads = 4;
        params.offset_ms = 0;
        params.no_context = true;
        params.single_segment = true;
        
        // Convert audio buffer to float
        std::vector<float> pcmf32(segment.size());
        for (size_t i = 0; i < segment.size(); i++) {
            pcmf32[i] = static_cast<float>(segment[i]) / 32768.0f;
        }
        
        // Run inference
        int ret = whisper_full(ctx_, params, pcmf32.data(), pcmf32.size());
        if (ret != 0) {
            std::ostringstream oss;
            oss << "whisper_full failed: " << ret;
            LOG_STT(oss.str());
            return result;
        }
        
        // Get result
        int n_segments = whisper_full_n_segments(ctx_);
        int total_tokens = 0;
        if (n_segments > 0) {
            std::string text;
            float total_prob = 0.0f;
            
            for (int i = 0; i < n_segments; i++) {
                const char* seg_text = whisper_full_get_segment_text(ctx_, i);
                text += seg_text;
                
                int n_tokens = whisper_full_n_tokens(ctx_, i);
                total_tokens += n_tokens;
                for (int j = 0; j < n_tokens; j++) {
                    float prob = whisper_full_get_token_p(ctx_, i, j);
                    total_prob += prob;
                }
            }
            
            result.text = text;
            result.confidence = total_tokens > 0 ? (total_prob / total_tokens) : 0.0f;
            result.token_count = total_tokens;
        }
        
        auto end = std::chrono::steady_clock::now();
        result.processing_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        return result;
    }
    
    bool is_ready() const {
        return ready_;
    }

private:
    STTConfig config_;
    whisper_context* ctx_;
    bool ready_;
};

STTEngine::STTEngine(const STTConfig& config) 
    : pimpl_(std::make_unique<Impl>(config)) {}

STTEngine::~STTEngine() = default;

Transcript STTEngine::transcribe(const AudioBuffer& segment) {
    return pimpl_->transcribe(segment);
}

bool STTEngine::is_ready() const {
    return pimpl_->is_ready();
}

} // namespace memo_rf
