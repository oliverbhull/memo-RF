#include "logger.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace memo_rf {

class Logger::Impl {
public:
    Impl(LogLevel min_level, const std::string& output_file) 
        : min_level_(min_level), file_stream_(nullptr) {
        if (!output_file.empty()) {
            file_stream_ = std::make_unique<std::ofstream>(output_file, std::ios::app);
            if (!file_stream_->is_open()) {
                std::cerr << "Warning: Failed to open log file: " << output_file << std::endl;
                file_stream_.reset();
            }
        }
    }
    
    ~Impl() {
        if (file_stream_ && file_stream_->is_open()) {
            file_stream_->close();
        }
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level < min_level_) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Format: [LEVEL] timestamp: message
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << "[" << level_string(level) << "] "
            << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count()
            << ": " << message;
        
        std::string formatted = oss.str();
        
        // Output to console (stderr for ERROR/WARN, stdout for INFO/DEBUG)
        if (level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
        
        // Output to file if configured
        if (file_stream_ && file_stream_->is_open()) {
            *file_stream_ << formatted << std::endl;
            file_stream_->flush();
        }
    }
    
    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        min_level_ = level;
    }
    
    LogLevel get_level() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return min_level_;
    }

private:
    mutable std::mutex mutex_;
    LogLevel min_level_;
    std::unique_ptr<std::ofstream> file_stream_;
    
    const char* level_string(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

std::unique_ptr<Logger::Impl> Logger::impl_ = nullptr;

void Logger::initialize(LogLevel min_level, const std::string& output_file) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>(min_level, output_file);
    }
}

void Logger::shutdown() {
    impl_.reset();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (impl_) {
        impl_->log(level, message);
    } else {
        // Fallback to console if not initialized
        if (level >= LogLevel::ERROR) {
            std::cerr << message << std::endl;
        } else {
            std::cout << message << std::endl;
        }
    }
}

const char* Logger::level_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::set_level(LogLevel level) {
    if (impl_) {
        impl_->set_level(level);
    }
}

LogLevel Logger::get_level() {
    if (impl_) {
        return impl_->get_level();
    }
    return LogLevel::INFO;
}

} // namespace memo_rf
