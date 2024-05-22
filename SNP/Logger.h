#pragma once
#include "common.h"

static constexpr auto FileDateFormat = "%d-%m-%Y_%Hh_%Mm";
static constexpr auto LogDateFormat = "%d-%m-%Y %H:%M:%S";

#define AnsiWhite   "\033[37m"
#define AnsiYellow  "\033[33m"
#define AnsiPurple  "\033[35m"
#define AnsiCyan    "\033[36m"
#define AnsiRed     "\033[31m"
#define AnsiBoldRed "\033[91m"
#define AnsiReset   "\033[0m"

enum class LogLevel {
    None,
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
    Trace
};

inline std::string to_string(LogLevel log_level) {
    switch (log_level) {
        case LogLevel::None:  return "None";
        case LogLevel::Fatal: return "Fatal";
        case LogLevel::Error: return "Error";
        case LogLevel::Warn:  return "Warn";
        case LogLevel::Info:  return "Info";
        case LogLevel::Debug: return "Debug";
        case LogLevel::Trace: return "Trace";
    }
    return std::format("LogLevel({})", (int)log_level);
}

// TODO: json map LogLevel to string

inline tm* get_local_time() {
    time_t current_time = time(nullptr);
    static tm result;
    localtime_s(&result, &current_time);
    return &result;
}

class LogFile {
public:
    LogFile(std::string_view name = "log") {
        open(name);
    }

    friend LogFile& operator<<(LogFile& out, const auto& value) {
        out.m_out << value << std::flush;

        return out;
    }

private:
    void open(std::string_view name) {
        if (m_out.is_open()) {
            return;
        }

        std::error_code ec;
        fs::create_directory("logs", ec);

        std::stringstream ss;
        const auto current_time = time(nullptr);
        ss << "logs/" << name << "_" << std::put_time(get_local_time(), FileDateFormat) << ".txt";
        m_out.open(ss.str(), std::ios::app);
    }

private:
    std::ofstream m_out;
};

class Logger {
public:
    Logger() = default;

    Logger(std::convertible_to<std::string> auto&&... prefixes)
        : m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    Logger(LogFile* log_file, std::convertible_to<std::string> auto&&... prefixes)
        : m_log_file{log_file}, m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    Logger(Logger& logger, std::convertible_to<std::string> auto&&... prefixes)
        : m_log_file{logger.m_log_file}, m_prefixes{logger.m_prefixes} {
        (m_prefixes.emplace_back(prefixes), ...);
    }

    void fatal(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Fatal) return;
        log(std::cerr, "Fatal", AnsiBoldRed, std::vformat(format, std::make_format_args(args...)));
        exit(-1);
    }

    void error(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Error) return;
        log(std::cerr, "Error", AnsiRed, std::vformat(format, std::make_format_args(args...)));
    }

    void warn(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Warn) return;
        log(std::cout, "Warn", AnsiYellow, std::vformat(format, std::make_format_args(args...)));
    }

    void info(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Info) return;
        log(std::cout, "Info", AnsiWhite, std::vformat(format, std::make_format_args(args...)));
    }

    void debug(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Debug) return;
        log(std::cerr, "Debug", AnsiPurple, std::vformat(format, std::make_format_args(args...)));
    }

    void trace(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Trace) return;
        log(std::cerr, "Trace", AnsiCyan, std::vformat(format, std::make_format_args(args...)));
    }

    static void set_log_level(LogLevel log_level) { s_log_level = log_level; }

private:
    void log(std::ostream& out, std::string_view log_level, std::string_view ansi_color, std::string_view string) {
        const auto prefix = make_prefix(log_level);

		out << ansi_color << prefix << string << AnsiReset << "\n";
        if (m_log_file) {
            *m_log_file << prefix << string << "\n";
        }
    }

    std::string make_prefix(std::string_view log_level) {
        std::stringstream ss;
        const auto current_time = time(nullptr);
        ss << "[" << std::put_time(get_local_time(), LogDateFormat) << "]";
        for (const std::string& prefix : m_prefixes) {
            ss << "[" << prefix << "]";
        }
        ss << "[" << log_level << "]";
        ss << " ";
        return ss.str();
    }

private:
    std::vector<std::string> m_prefixes;
    LogFile* m_log_file = nullptr;
    inline static LogLevel s_log_level = LogLevel::Info;
};

inline LogFile g_main_log{"CrownLink"};
inline Logger g_root_logger{&g_main_log};