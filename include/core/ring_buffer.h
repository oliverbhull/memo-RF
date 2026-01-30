#pragma once

/**
 * @file ring_buffer.h
 * @brief Lock-free ring buffer for audio processing
 *
 * Used for:
 * - Pre-speech buffering (capture audio before speech detection)
 * - Audio I/O buffering
 * - Inter-thread communication
 */

#include "types.h"
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace memo_rf {

/**
 * @brief Fixed-size ring buffer for audio samples
 *
 * Thread-safe for single-producer single-consumer usage.
 * Optimized for audio processing with bulk read/write operations.
 */
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity)
        , capacity_(capacity)
        , write_pos_(0)
        , read_pos_(0)
        , size_(0) {}

    /// Get buffer capacity
    size_t capacity() const { return capacity_; }

    /// Get current number of items in buffer
    size_t size() const { return size_.load(std::memory_order_acquire); }

    /// Check if buffer is empty
    bool empty() const { return size() == 0; }

    /// Check if buffer is full
    bool full() const { return size() == capacity_; }

    /// Available space for writing
    size_t available() const { return capacity_ - size(); }

    /**
     * @brief Write items to buffer
     * @param data Source data
     * @param count Number of items to write
     * @return Number of items actually written
     */
    size_t write(const T* data, size_t count) {
        size_t avail = available();
        size_t to_write = std::min(count, avail);

        if (to_write == 0) return 0;

        size_t write_idx = write_pos_.load(std::memory_order_relaxed);

        // First chunk: from write position to end of buffer
        size_t first_chunk = std::min(to_write, capacity_ - write_idx);
        std::copy(data, data + first_chunk, buffer_.begin() + write_idx);

        // Second chunk: wrap around to beginning
        if (to_write > first_chunk) {
            std::copy(data + first_chunk, data + to_write, buffer_.begin());
        }

        // Update write position
        write_pos_.store((write_idx + to_write) % capacity_, std::memory_order_relaxed);
        size_.fetch_add(to_write, std::memory_order_release);

        return to_write;
    }

    /// Write a vector
    size_t write(const std::vector<T>& data) {
        return write(data.data(), data.size());
    }

    /**
     * @brief Read items from buffer
     * @param data Destination buffer
     * @param count Maximum items to read
     * @return Number of items actually read
     */
    size_t read(T* data, size_t count) {
        size_t avail = size();
        size_t to_read = std::min(count, avail);

        if (to_read == 0) return 0;

        size_t read_idx = read_pos_.load(std::memory_order_relaxed);

        // First chunk: from read position to end of buffer
        size_t first_chunk = std::min(to_read, capacity_ - read_idx);
        std::copy(buffer_.begin() + read_idx, buffer_.begin() + read_idx + first_chunk, data);

        // Second chunk: wrap around to beginning
        if (to_read > first_chunk) {
            std::copy(buffer_.begin(), buffer_.begin() + (to_read - first_chunk), data + first_chunk);
        }

        // Update read position
        read_pos_.store((read_idx + to_read) % capacity_, std::memory_order_relaxed);
        size_.fetch_sub(to_read, std::memory_order_release);

        return to_read;
    }

    /// Read into a vector (resizes vector to actual read count)
    size_t read(std::vector<T>& data, size_t max_count) {
        data.resize(max_count);
        size_t actual = read(data.data(), max_count);
        data.resize(actual);
        return actual;
    }

    /**
     * @brief Peek at items without removing them
     * @param data Destination buffer
     * @param count Maximum items to peek
     * @return Number of items actually peeked
     */
    size_t peek(T* data, size_t count) const {
        size_t avail = size();
        size_t to_peek = std::min(count, avail);

        if (to_peek == 0) return 0;

        size_t read_idx = read_pos_.load(std::memory_order_relaxed);

        // First chunk
        size_t first_chunk = std::min(to_peek, capacity_ - read_idx);
        std::copy(buffer_.begin() + read_idx, buffer_.begin() + read_idx + first_chunk, data);

        // Second chunk
        if (to_peek > first_chunk) {
            std::copy(buffer_.begin(), buffer_.begin() + (to_peek - first_chunk), data + first_chunk);
        }

        return to_peek;
    }

    /**
     * @brief Get all data as a vector (peek, doesn't consume)
     */
    std::vector<T> peek_all() const {
        std::vector<T> result(size());
        peek(result.data(), result.size());
        return result;
    }

    /**
     * @brief Skip items without reading
     * @param count Number of items to skip
     * @return Number of items actually skipped
     */
    size_t skip(size_t count) {
        size_t avail = size();
        size_t to_skip = std::min(count, avail);

        if (to_skip == 0) return 0;

        size_t read_idx = read_pos_.load(std::memory_order_relaxed);
        read_pos_.store((read_idx + to_skip) % capacity_, std::memory_order_relaxed);
        size_.fetch_sub(to_skip, std::memory_order_release);

        return to_skip;
    }

    /// Clear all data
    void clear() {
        read_pos_.store(0, std::memory_order_relaxed);
        write_pos_.store(0, std::memory_order_relaxed);
        size_.store(0, std::memory_order_release);
    }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<size_t> write_pos_;
    std::atomic<size_t> read_pos_;
    std::atomic<size_t> size_;
};

/// Convenience alias for audio sample ring buffer
using AudioRingBuffer = RingBuffer<Sample>;

} // namespace memo_rf
