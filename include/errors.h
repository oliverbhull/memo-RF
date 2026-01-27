#pragma once

#include <string>
#include <variant>
#include <optional>

namespace memo_rf {

/**
 * @brief Error types for different failure modes
 */
enum class ErrorType {
    None,
    IOError,
    NetworkError,
    ParseError,
    InvalidState,
    ResourceError,
    Timeout,
    Unknown
};

/**
 * @brief Error information structure
 */
struct Error {
    ErrorType type = ErrorType::None;
    std::string message;
    
    Error() = default;
    Error(ErrorType t, const std::string& msg) : type(t), message(msg) {}
    
    bool is_error() const { return type != ErrorType::None; }
    operator bool() const { return is_error(); }
};

/**
 * @brief Result type for operations that can fail
 * 
 * Similar to Rust's Result<T, E> or C++23's std::expected.
 * Holds either a value of type T or an Error.
 */
template<typename T>
class Result {
public:
    // Construct from value (success)
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    
    // Construct from error
    Result(const Error& error) : data_(error) {}
    Result(Error&& error) : data_(std::move(error)) {}
    
    // Check if result is success
    bool is_ok() const {
        return std::holds_alternative<T>(data_);
    }
    
    // Check if result is error
    bool is_error() const {
        return std::holds_alternative<Error>(data_);
    }
    
    // Get value (throws if error)
    const T& value() const {
        if (!is_ok()) {
            throw std::runtime_error("Result is error, cannot get value");
        }
        return std::get<T>(data_);
    }
    
    T& value() {
        if (!is_ok()) {
            throw std::runtime_error("Result is error, cannot get value");
        }
        return std::get<T>(data_);
    }
    
    // Get error (throws if success)
    const Error& error() const {
        if (is_ok()) {
            throw std::runtime_error("Result is success, cannot get error");
        }
        return std::get<Error>(data_);
    }
    
    // Get value or default
    T value_or(const T& default_value) const {
        return is_ok() ? std::get<T>(data_) : default_value;
    }
    
    // Get value or default (moved)
    T value_or(T&& default_value) const {
        return is_ok() ? std::get<T>(data_) : std::move(default_value);
    }
    
    // Operator bool (true if success)
    explicit operator bool() const {
        return is_ok();
    }

private:
    std::variant<T, Error> data_;
};

// Specialization for void (success/failure only)
template<>
class Result<void> {
public:
    Result() : is_ok_(true) {}
    Result(const Error& error) : is_ok_(false), error_(error) {}
    Result(Error&& error) : is_ok_(false), error_(std::move(error)) {}
    
    bool is_ok() const { return is_ok_; }
    bool is_error() const { return !is_ok_; }
    const Error& error() const { return error_; }
    
    explicit operator bool() const { return is_ok_; }

private:
    bool is_ok_;
    Error error_;
};

// Helper functions for creating errors
inline Error make_error(ErrorType type, const std::string& message) {
    return Error(type, message);
}

inline Error make_io_error(const std::string& message) {
    return Error(ErrorType::IOError, message);
}

inline Error make_network_error(const std::string& message) {
    return Error(ErrorType::NetworkError, message);
}

inline Error make_parse_error(const std::string& message) {
    return Error(ErrorType::ParseError, message);
}

inline Error make_timeout_error(const std::string& message = "Operation timed out") {
    return Error(ErrorType::Timeout, message);
}

} // namespace memo_rf
