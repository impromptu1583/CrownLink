#pragma once
#include <iostream>
#include <fstream>
#include <format>
#include <filesystem>
namespace fs = std::filesystem;

static constexpr auto FileDateFormat = "%d-%m-%Y;%H-%M-%S";
static constexpr auto LogDateFormat = "%d-%m-%Y %H:%M:%S";

#define AnsiWhite   "\033[37m"
#define AnsiYellow  "\033[33m"
#define AnsiPurple  "\033[35m"
#define AnsiRed     "\033[31m"
#define AnsiBoldRed "\033[91m"
#define AnsiReset   "\033[0m"

class LogFile {
public:
    LogFile(std::string_view name = "log") {
        open(name);
    }

    friend LogFile& operator<<(LogFile& out, const auto& value) {
        out.m_out << value;
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
        ss << "logs/" << name << std::put_time(localtime(&current_time), FileDateFormat) << ".txt";
        m_out.open(ss.str(), std::ios::app);
    }

private:
    std::ofstream m_out;
};

class Logger {
public:
    Logger(std::convertible_to<std::string> auto&&... prefixes) : m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    Logger(LogFile* log_file, std::convertible_to<std::string> auto&&... prefixes) : m_log_file{log_file}, m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    void info(std::string_view format, const auto&... args) {
        log(std::cout, "Info", AnsiWhite, std::vformat(format, std::make_format_args(args...)));
    }

    void warn(std::string_view format, const auto&... args) {
        log(std::cout, "Warn", AnsiYellow, std::vformat(format, std::make_format_args(args...)));
    }

    void debug(std::string_view format, const auto&... args) {
        log(std::cerr, "Debug", AnsiPurple, std::vformat(format, std::make_format_args(args...)));
    }

    void error(std::string_view format, const auto&... args) {
        log(std::cerr, "Error", AnsiRed, std::vformat(format, std::make_format_args(args...)));
    }

    void fatal(std::string_view format, const auto&... args) {
        log(std::cerr, "Fatal", AnsiBoldRed, std::vformat(format, std::make_format_args(args...)));
        exit(-1);
    }

    Logger operator[](std::string sv) {
        Logger copy = *this;
        copy.m_prefixes.push_back(std::move(sv));
        return copy;
    }

    inline void set_cout_enabled(bool enabled) { m_cout_enabled = enabled; }

private:
    void log(std::ostream& out, std::string_view log_level, std::string_view ansi_color, std::string_view string) {
        const auto prefix = make_prefix(log_level);

        if (m_cout_enabled) {
            out << ansi_color << prefix << string << AnsiReset << "\n";
        }
        if (m_log_file) {
            *m_log_file << prefix << string << "\n";
        }
    }

    std::string make_prefix(std::string_view log_level) {
        std::stringstream ss;
        const auto current_time = time(nullptr);
        ss << "[" << std::put_time(localtime(&current_time), LogDateFormat) << "]";
        for (const std::string& prefix : m_prefixes) {
            ss << "[" << prefix << "]";
        }
        ss << "[" << log_level << "]";
        ss << " ";
        return ss.str();
    }

private:
    bool m_cout_enabled = true;
    std::vector<std::string> m_prefixes;
    LogFile* m_log_file;
};