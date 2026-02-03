#pragma once

#include <string>
#include <ostream>
#include <memory>

namespace memo_rf {

/**
 * @brief Log levels for filtering output
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

/**
 * @brief Lightweight, thread-safe logging system
 * 
 * Provides structured logging with levels and optional file output.
 * Thread-safe for concurrent use from multiple threads.
 */
class Logger {
public:
    /**
     * @brief Initialize logger with minimum log level
     * @param min_level Minimum level to output (default: INFO)
     * @param output_file Optional file path for log output (empty = console only)
     */
    static void initialize(LogLevel min_level = LogLevel::INFO, 
                          const std::string& output_file = "");
    
    /**
     * @brief Shutdown logger and close file handles
     */
    static void shutdown();
    
    /**
     * @brief Log a message at DEBUG level
     */
    static void debug(const std::string& message);
    
    /**
     * @brief Log a message at INFO level
     */
    static void info(const std::string& message);
    
    /**
     * @brief Log a message at WARN level
     */
    static void warn(const std::string& message);
    
    /**
     * @brief Log a message at ERROR level
     */
    static void error(const std::string& message);
    
    /**
     * @brief Set minimum log level (filters output)
     */
    static void set_level(LogLevel level);
    
    /**
     * @brief Get current minimum log level
     */
    static LogLevel get_level();

private:
    class Impl;
    static std::unique_ptr<Impl> impl_;
    
    static void log(LogLevel level, const std::string& message);
    static const char* level_string(LogLevel level);
};

// Convenience macros for component-specific logging
#define LOG_DEBUG(msg) memo_rf::Logger::debug("[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "] " + msg)
#define LOG_INFO(msg) memo_rf::Logger::info(msg)
#define LOG_WARN(msg) memo_rf::Logger::warn(msg)
#define LOG_ERROR(msg) memo_rf::Logger::error(msg)

// Component-specific logging macros
#define LOG_AUDIO(msg) memo_rf::Logger::debug(std::string("[Audio] ") + (msg))
#define LOG_VAD(msg) memo_rf::Logger::debug(std::string("[VAD] ") + (msg))
#define LOG_STT(msg) memo_rf::Logger::info(std::string("[STT] ") + (msg))
#define LOG_ROUTER(msg) memo_rf::Logger::info(std::string("[Router] ") + (msg))
#define LOG_LLM(msg) memo_rf::Logger::info(std::string("[LLM] ") + (msg))
#define LOG_TTS(msg) memo_rf::Logger::info(std::string("[TTS] ") + (msg))
#define LOG_TX(msg) memo_rf::Logger::info(std::string("[TX] ") + (msg))
#define LOG_TRACE(utterance_id, stage, data) memo_rf::Logger::info(std::string("[trace] utterance_id=") + std::to_string(utterance_id) + " stage=" + (stage) + " " + (data))

} // namespace memo_rf
