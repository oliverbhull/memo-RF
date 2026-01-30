#ifdef USE_AGENTS_SDK

#include "agents_sdk_bridge.h"
#include "config.h"
#include "logger.h"
#include <agents-cpp/context.h>
#include <agents-cpp/llm_interface.h>
#include <agents-cpp/agents/actor_agent.h>
#include <agents-cpp/coroutine_utils.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace memo_rf {

namespace {

/** Derive Ollama API base URL from memo-RF endpoint. SDK expects base including /api (e.g. .../api/chat -> .../api). */
std::string ollama_base_from_endpoint(const std::string& endpoint) {
    if (endpoint.empty()) return "http://localhost:11434/api";
    size_t last = endpoint.rfind('/');
    if (last != std::string::npos && last > 8) {
        return endpoint.substr(0, last);
    }
    return endpoint;
}

/** Parse .env file into key=value map. Strips surrounding quotes and skips comments/empty lines. */
std::map<std::string, std::string> parse_env_file(const std::string& path) {
    std::map<std::string, std::string> out;
    std::ifstream f(path);
    if (!f.is_open()) return out;
    std::string line;
    while (std::getline(f, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;
        size_t eq = line.find('=', start);
        if (eq == std::string::npos) continue;
        std::string key = line.substr(start, eq - start);
        std::string value = line.substr(eq + 1);
        // Trim key
        size_t key_end = key.find_last_not_of(" \t");
        if (key_end != std::string::npos) key = key.substr(0, key_end + 1);
        // Trim value and strip quotes
        size_t val_start = value.find_first_not_of(" \t");
        if (val_start != std::string::npos) value = value.substr(val_start);
        size_t val_end = value.find_last_not_of(" \t\r\n");
        if (val_end != std::string::npos) value = value.substr(0, val_end + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
        out[key] = value;
    }
    return out;
}

/** Ensure Ollama base URL ends with /api (SDK expects base that includes /api). */
std::string normalize_ollama_base(const std::string& base) {
    if (base.empty()) return "http://localhost:11434/api";
    std::string b = base;
    while (!b.empty() && (b.back() == '/' || b.back() == ' ')) b.pop_back();
    if (b.empty()) return "http://localhost:11434/api";
    if (b.size() >= 4 && b.compare(b.size() - 4, 4, "/api") == 0) return b;
    return b + "/api";
}

// ----- Manufacturing stub tools (in-memory, thread-safe) -----
struct WorkOrderRecord {
    int ticket_id = 0;
    std::string machine;
    std::string description;
    std::string status = "dispatched";
};

struct ManufacturingStubs {
    std::mutex mtx;
    int ticket_counter = 400;
    std::map<std::string, WorkOrderRecord> machine_to_last_work_order;
    std::map<std::string, int> run_counts;  // line id -> count
    std::vector<std::string> safety_incidents;

    int log_work_order(const std::string& machine, const std::string& description) {
        std::lock_guard<std::mutex> lock(mtx);
        ++ticket_counter;
        WorkOrderRecord rec;
        rec.ticket_id = ticket_counter;
        rec.machine = machine.empty() ? "unknown" : machine;
        rec.description = description;
        rec.status = "dispatched";
        machine_to_last_work_order[rec.machine] = rec;
        return rec.ticket_id;
    }

    int get_run_count(const std::string& line) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string key = line.empty() ? "line 1" : line;
        auto it = run_counts.find(key);
        if (it != run_counts.end()) return it->second;
        run_counts[key] = 420;  // default
        return 420;
    }

    void log_safety_incident(const std::string& area, const std::string& description) {
        std::lock_guard<std::mutex> lock(mtx);
        safety_incidents.push_back(area + ": " + description);
    }

    std::string get_work_order_status(const std::string& machine_or_ticket) {
        std::lock_guard<std::mutex> lock(mtx);
        // Try as ticket id (numeric)
        bool numeric = true;
        for (char c : machine_or_ticket) {
            if (!std::isdigit(static_cast<unsigned char>(c))) { numeric = false; break; }
        }
        if (numeric && !machine_or_ticket.empty()) {
            int tid = std::stoi(machine_or_ticket);
            for (const auto& [m, rec] : machine_to_last_work_order) {
                if (rec.ticket_id == tid) {
                    return rec.status + ", ETA 15 minutes";
                }
            }
        }
        // Try as machine name
        auto it = machine_to_last_work_order.find(machine_or_ticket);
        if (it != machine_to_last_work_order.end()) {
            return "Ticket " + std::to_string(it->second.ticket_id) + " " + it->second.status + ". ETA 15 minutes.";
        }
        return "No open work order for that machine or ticket.";
    }
};

ManufacturingStubs& get_manufacturing_stubs() {
    static ManufacturingStubs stubs;
    return stubs;
}

/** Ensure radio-friendly reply: one short sentence, ends with . Over. */
std::string radio_over(const std::string& s) {
    std::string t = s;
    while (!t.empty() && (t.back() == ' ' || t.back() == '.')) t.pop_back();
    if (t.empty()) return "Stand by. Over.";
    auto ends_with = [&t](const std::string& suffix) {
        return t.size() >= suffix.size() &&
               t.compare(t.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (ends_with("Over.") || ends_with("over.") || ends_with("over")) return t + (t.back() == '.' ? "" : ".");
    return t + ". Over.";
}

/** Return lowercase copy of s for comparison. */
std::string to_lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

/** Parse LLM classification response to one of: maintenance, production, safety, status. */
std::string parse_manufacturing_category(const std::string& llm_response) {
    std::string lower = to_lower(llm_response);
    if (lower.find("maintenance") != std::string::npos) return "maintenance";
    if (lower.find("production") != std::string::npos) return "production";
    if (lower.find("safety") != std::string::npos) return "safety";
    if (lower.find("status") != std::string::npos) return "status";
    return "";
}

/** Run manufacturing floor router: classify via LLM, dispatch to stubs, return radio reply. */
std::string run_manufacturing_router(
    std::shared_ptr<agents::LLMInterface> llm,
    const std::string& transcript)
{
    ManufacturingStubs& stubs = get_manufacturing_stubs();

    std::string classify_prompt =
        "Classify this factory floor message into exactly one category: maintenance, production, safety, or status. "
        "Reply with only that one word. Message: \"" + transcript + "\"";
    agents::LLMResponse resp = llm->chat(classify_prompt);
    std::string category = parse_manufacturing_category(resp.content);
    if (category.empty()) category = "default";

    Logger::info("Manufacturing route: " + category + " - " + transcript);

    if (category == "maintenance") {
        int ticket_id = stubs.log_work_order("", transcript);
        return radio_over("Work order " + std::to_string(ticket_id) + " logged. Maintenance notified. Over.");
    }
    if (category == "production") {
        int count = stubs.get_run_count("line 1");
        return radio_over("Run count " + std::to_string(count) + ". Over.");
    }
    if (category == "safety") {
        stubs.log_safety_incident("floor", transcript);
        return radio_over("Incident logged. Safety notified. Over.");
    }
    if (category == "status") {
        std::string status = stubs.get_work_order_status(transcript);
        return radio_over(status);
    }
    return "Stand by. Over.";
}

} // namespace

struct AgentsSdkBridge::Impl {
    std::shared_ptr<agents::Context> context;
    std::shared_ptr<agents::LLMInterface> llm;
    std::unique_ptr<agents::ActorAgent> agent;
    bool use_manufacturing_router = false;
};

AgentsSdkBridge::AgentsSdkBridge() = default;

AgentsSdkBridge::~AgentsSdkBridge() = default;

bool AgentsSdkBridge::init(const Config& config) {
    impl_ = std::make_unique<Impl>();

    std::string model = config.llm.model_name;
    std::string base = ollama_base_from_endpoint(config.llm.endpoint);

    if (!config.llm.agents_sdk_env_path.empty()) {
        auto env = parse_env_file(config.llm.agents_sdk_env_path);
        if (!env.empty()) {
            auto it_url = env.find("OLLAMA_BASE_URL");
            auto it_model = env.find("MODEL");
            if (it_url != env.end()) base = normalize_ollama_base(it_url->second);
            if (it_model != env.end()) model = it_model->second;
            Logger::info("AgentsSdkBridge: using agents-sdk .env (base=" + base + ", model=" + model + ")");
        } else {
            Logger::warn("AgentsSdkBridge: agents_sdk_env_path set but could not read file: " + config.llm.agents_sdk_env_path);
        }
    }

    auto llm = agents::createLLM("ollama", "", model);
    llm->setApiBase(base);

    agents::LLMOptions opts;
    opts.temperature = static_cast<double>(config.llm.temperature);
    opts.max_tokens = config.llm.max_tokens;
    opts.timeout_ms = config.llm.timeout_ms;
    if (!config.llm.stop_sequences.empty()) {
        opts.stop_sequences = config.llm.stop_sequences;
    }
    llm->setOptions(opts);

    impl_->context = std::make_shared<agents::Context>();
    impl_->context->setLLM(llm);
    impl_->context->setSystemPrompt(config.llm.system_prompt);
    impl_->llm = llm;
    impl_->use_manufacturing_router = config.llm.use_manufacturing_router;

    if (impl_->use_manufacturing_router) {
        Logger::info("AgentsSdkBridge initialized (manufacturing router, Ollama " + model + ")");
    } else {
        if (!config.tools.enabled.empty()) {
            Logger::info("AgentsSdkBridge: tools requested but not registered (use legacy build for tools)");
        }
        impl_->agent = std::make_unique<agents::ActorAgent>(impl_->context);
        impl_->agent->setAgentPrompt(config.llm.system_prompt);
        impl_->agent->init();
        Logger::info("AgentsSdkBridge initialized (Ollama " + model + ")");
    }

    return true;
}

std::string AgentsSdkBridge::run(const std::string& transcript) {
    if (!impl_) {
        Logger::error("AgentsSdkBridge: not initialized");
        return "Stand by.";
    }

    if (impl_->use_manufacturing_router) {
        try {
            return run_manufacturing_router(impl_->llm, transcript);
        } catch (const std::exception& e) {
            Logger::error("AgentsSdkBridge: " + std::string(e.what()));
            return "Stand by. Over.";
        }
    }

    if (!impl_->agent) {
        Logger::error("AgentsSdkBridge: not initialized");
        return "Stand by.";
    }
    try {
        agents::JsonObject result = agents::blockingWait(impl_->agent->run(transcript));
        if (result.contains("answer") && result["answer"].is_string()) {
            return result["answer"].get<std::string>();
        }
        if (result.contains("response") && result["response"].is_string()) {
            return result["response"].get<std::string>();
        }
        Logger::warn("AgentsSdkBridge: result missing 'answer' or 'response'");
        return "Stand by.";
    } catch (const std::exception& e) {
        Logger::error("AgentsSdkBridge: " + std::string(e.what()));
        return "Stand by.";
    }
}

} // namespace memo_rf

#endif // USE_AGENTS_SDK
