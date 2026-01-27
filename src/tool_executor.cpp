#include "tool_executor.h"
#include "logger.h"
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace memo_rf {

ToolExecutor::ToolExecutor(ToolRegistry* registry, size_t max_concurrent)
    : registry_(registry), max_concurrent_(max_concurrent), running_(true), active_executions_(0) {
    
    // Start worker threads
    for (size_t i = 0; i < max_concurrent_; ++i) {
        worker_threads_.emplace_back(&ToolExecutor::worker_thread, this);
    }
}

ToolExecutor::~ToolExecutor() {
    shutdown();
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool ToolExecutor::execute_async(const ToolExecutionRequest& call, 
                                 ToolExecutionCallback callback,
                                 int timeout_ms) {
    if (!running_) {
        Logger::warn("ToolExecutor is shutdown, cannot execute tool: " + call.tool_name);
        return false;
    }
    
    if (!registry_ || !registry_->has_tool(call.tool_name)) {
        Logger::error("Tool not found: " + call.tool_name);
        if (callback) {
            ToolExecutionResult result;
            result.tool_call_id = call.tool_call_id;
            result.result = ToolResult::error_result("Tool not found: " + call.tool_name);
            callback(result);
        }
        return false;
    }
    
    ExecutionTask task;
    task.call = call;
    task.callback = callback;
    task.timeout_ms = timeout_ms;
    task.start_time = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    
    queue_cv_.notify_one();
    return true;
}

ToolExecutionResult ToolExecutor::execute_sync(const ToolExecutionRequest& call, int timeout_ms) {
    ToolExecutionResult result;
    result.tool_call_id = call.tool_call_id;
    
    std::mutex result_mutex;
    std::condition_variable result_cv;
    bool completed = false;
    
    auto callback = [&](const ToolExecutionResult& res) {
        std::lock_guard<std::mutex> lock(result_mutex);
        result = res;
        completed = true;
        result_cv.notify_one();
    };
    
    if (!execute_async(call, callback, timeout_ms)) {
        result.result = ToolResult::error_result("Failed to queue tool execution");
        return result;
    }
    
    // Wait for completion
    std::unique_lock<std::mutex> lock(result_mutex);
    if (timeout_ms > 0) {
        auto timeout = std::chrono::milliseconds(timeout_ms);
        result_cv.wait_for(lock, timeout, [&] { return completed; });
        if (!completed) {
            result.result = ToolResult::error_result("Tool execution timeout");
        }
    } else {
        result_cv.wait(lock, [&] { return completed; });
    }
    
    return result;
}

bool ToolExecutor::is_idle() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.empty() && active_executions_ == 0;
}

size_t ToolExecutor::pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return task_queue_.size() + active_executions_;
}

bool ToolExecutor::wait_for_completion(int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        if (is_idle()) {
            return true;
        }
        
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ToolExecutor::shutdown() {
    running_ = false;
    queue_cv_.notify_all();
}

void ToolExecutor::worker_thread() {
    while (running_) {
        ExecutionTask task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { 
                return !task_queue_.empty() || !running_; 
            });
            
            if (!running_ && task_queue_.empty()) {
                break;
            }
            
            if (task_queue_.empty()) {
                continue;
            }
            
            task = task_queue_.front();
            task_queue_.pop();
            active_executions_++;
        }
        
        execute_tool_with_timeout(task);
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            active_executions_--;
        }
    }
}

void ToolExecutor::execute_tool_with_timeout(const ExecutionTask& task) {
    auto tool = registry_->get_tool(task.call.tool_name);
    if (!tool) {
        Logger::error("Tool not found during execution: " + task.call.tool_name);
        if (task.callback) {
            ToolExecutionResult result;
            result.tool_call_id = task.call.tool_call_id;
            result.result = ToolResult::error_result("Tool not found");
            task.callback(result);
        }
        return;
    }
    
    ToolExecutionResult result;
    result.tool_call_id = task.call.tool_call_id;
    
    // Check timeout before execution
    if (task.timeout_ms > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - task.start_time).count();
        if (elapsed >= task.timeout_ms) {
            result.result = ToolResult::error_result("Tool execution timeout");
            if (task.callback) {
                task.callback(result);
            }
            return;
        }
    }
    
    // Execute tool
    try {
        result.result = tool->execute(task.call.params_json);
    } catch (const std::exception& e) {
        Logger::error("Tool execution exception for " + task.call.tool_name + ": " + e.what());
        result.result = ToolResult::error_result("Tool execution exception: " + std::string(e.what()));
    } catch (...) {
        Logger::error("Unknown exception during tool execution: " + task.call.tool_name);
        result.result = ToolResult::error_result("Unknown tool execution error");
    }
    
    // Call callback
    if (task.callback) {
        task.callback(result);
    }
}

} // namespace memo_rf
