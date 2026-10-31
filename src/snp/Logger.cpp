#include "Logger.h"

#include <mutex>
#include <utility>

#include "Config.h"

namespace {

// Heap-held so the CRT never destroys the logger at process exit: by then ExitProcess has killed the other
// threads, and one of them may still hold a sink or stream lock
std::shared_ptr<spdlog::logger>* g_logger_keep_alive = nullptr;

spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info: return spdlog::level::info;
        case LogLevel::Warn: return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Fatal: return spdlog::level::critical;
        case LogLevel::None: return spdlog::level::off;
    }
    return spdlog::level::debug;
}

}  // namespace

void ensure_logging() {
    static std::once_flag once;
    std::call_once(once, [] {
        try {
            const auto log_dir = g_starcraft_dir / "crownlink_logs";

            auto standard_sink =
                std::make_shared<spdlog::sinks::daily_file_sink_mt>((log_dir / "CrownLink.txt").generic_string(), 2, 30);
            standard_sink->set_level(spdlog::level::debug);

            auto trace_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                (log_dir / "CLTrace.txt").generic_string(), 1024 * 1024 * 10, 3
            );
            trace_sink->set_level(spdlog::level::trace);

            auto logger = std::make_shared<spdlog::logger>(
                "CL", spdlog::sinks_init_list{std::move(standard_sink), std::move(trace_sink)}
            );
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::warn);
            spdlog::set_default_logger(logger);
            g_logger_keep_alive = new std::shared_ptr<spdlog::logger>{std::move(logger)};

            const auto log_level = SnpConfig::instance().log_level;
            spdlog::set_level(to_spdlog_level(log_level));
            if (log_level != LogLevel::Trace && log_level != LogLevel::Debug) {
                spdlog::enable_backtrace(32);
            }
        } catch (const std::exception&) {
            // Nowhere to report this, and it must not propagate into storm
        }
    });
}

void release_logging() {
    if (!g_logger_keep_alive) {
        return;
    }
    (*g_logger_keep_alive)->flush();
    delete std::exchange(g_logger_keep_alive, nullptr);
}
