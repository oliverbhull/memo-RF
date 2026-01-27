#pragma once

#include "tool.h"
#include "tool_registry.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace memo_rf {

/**
 * @brief Tool execution request structure
 */
struct ToolExecutionRequest {
    std::string tool_name;
    std::string tool_call_id;  // ID from LLM response
    std::string params_json;   // Parameters as JSON string
};

/**
 * @brief Tool execution result with call ID
 */
struct ToolExecutionResult {
    std::string tool_call_id;
    ToolResult result;
};

/**
 * @brief Callback type for tool execution completion
 */
using ToolExecutionCallback = std::function<void(const ToolExecutionResult&)>;

/**
 * @brief Async tool execution engine
 * 
 * Executes tools asynchronously without blocking the main audio processing loop.
 * Supports queuing, timeout handling, and result callbacks.
 */
class ToolExecutor {
public:
    /**
     * @brief Construct tool executor with registry
     * @param registry Tool registry containing available tools
     * @param max_concurrent Maximum number of concurrent tool executions
     */
    explicit ToolExecutor(ToolRegistry* registry, size_t max_concurrent = 1);
    
    /**
     * @brief Destructor - ensures all pending executions complete
     */
    ~ToolExecutor();
    
    // Non-copyable
    ToolExecutor(const ToolExecutor&) = delete;
    ToolExecutor& operator=(const ToolExecutor&) = delete;
    
    /**
     * @brief Execute a tool asynchronously
     * @param call Tool call request
     * @param callback Callback function called when execution completes
     * @param timeout_ms Timeout in milliseconds (0 = no timeout)
     * @return true if tool execution queued, false if error
     */
    bool execute_async(const ToolExecutionRequest& call, 
                      ToolExecutionCallback callback,
                      int timeout_ms = 0);
    
    /**
     * @brief Execute a tool synchronously (blocks until complete)
     * @param call Tool call request
     * @param timeout_ms Timeout in milliseconds (0 = no timeout)
     * @return Tool execution result
     */
    ToolExecutionResult execute_sync(const ToolExecutionRequest& call, int timeout_ms = 0);
    
    /**
     * @brief Check if executor is idle (no pending executions)
     * @return true if no tools are executing or queued
     */
    bool is_idle() const;
    
    /**
     * @brief Get number of pending tool executions
     * @return Count of queued and executing tools
     */
    size_t pending_count() const;
    
    /**
     * @brief Wait for all pending executions to complete
     * @param timeout_ms Maximum time to wait (0 = wait indefinitely)
     * @return true if all completed, false if timeout
     */
    bool wait_for_completion(int timeout_ms = 0);
    
    /**
     * @brief Shutdown executor (stop accepting new executions)
     */
    void shutdown();

private:
    struct ExecutionTask {
        ToolExecutionRequest call;
        ToolExecutionCallback callback;
        int timeout_ms;
        std::chrono::steady_clock::time_point start_time;
    };
    
    void worker_thread();
    void execute_tool_with_timeout(const ExecutionTask& task);
    
    ToolRegistry* registry_;
    size_t max_concurrent_;
    std::atomic<bool> running_;
    std::atomic<size_t> active_executions_;
    
    std::queue<ExecutionTask> task_queue_;
    mutable std::mutex queue_mutex_;  // mutable to allow locking in const methods
    std::condition_variable queue_cv_;
    
    std::vector<std::thread> worker_threads_;
    std::mutex result_mutex_;
};

} // namespace memo_rf
