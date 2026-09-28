#pragma once

#include <rfl/json.hpp>

#include <filesystem>
#include <fstream>
#include <ostream>
#include <spdlog.h>
#include <string>

#include "Logger.h"
#include "shared/types.h"

namespace fs = std::filesystem;

enum class LogLevel {
    None,
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
    Trace
};

namespace rfl {

// log_level is stored as its integer value in CrownLink.json; reflect-cpp
// would otherwise write the enumerator name as a string
template <>
struct Reflector<::LogLevel> {
    using ReflType = u32;
    static ::LogLevel to(const ReflType& value) noexcept { return static_cast<::LogLevel>(value); }
    static ReflType from(const ::LogLevel& value) noexcept { return std::to_underlying(value); }
};

}  // namespace rfl

struct SnpConfig {
    u32 config_version = 2;
    std::string server = "crowserve.cosmonarchy.com";
    std::string port = "33377";
    std::string lobby_password = "";
    bool add_map_to_lobby_name = true;

    LogLevel log_level = LogLevel::Debug;

    static SnpConfig& instance();
};

class SnpConfigLoader {
public:
    SnpConfigLoader(const std::string& filename) : m_path{g_starcraft_dir / filename} {}

    SnpConfig load() {
        std::string contents;
        if (std::ifstream file{m_path}; file.good()) {
            contents = std::string{std::istreambuf_iterator<char>(file), {}};
            m_config_existed = true;
            spdlog::info("Config file loaded, contents: {}", contents);
        } else {
            spdlog::warn("Config file not found, defaults will be used");
        }

        SnpConfig config{};
        if (m_config_existed) {
            // Missing keys fall back to defaults; a wrong-typed value fails the
            // whole read, which also falls back to defaults and rewrites the file
            const auto loaded = rfl::json::read<SnpConfig, rfl::DefaultIfMissing>(contents);
            if (!loaded) {
                spdlog::warn("Config file error: {}", loaded.error().what());
            } else if (loaded.value().config_version == config.config_version) {
                config = loaded.value();
            } else {
                spdlog::warn(
                    "Config version mismatch: file has {}, expected {}", loaded.value().config_version,
                    config.config_version
                );
            }
        }

        save(config);
        return config;
    }

    void save(const SnpConfig& config) {
        if (m_path.empty()) {
            return;
        }

        if (m_config_existed) {
            spdlog::info("Updating config at {}", m_path.string());
        } else {
            spdlog::info("Creating new config at {}", m_path.string());
        }

        std::ofstream file{m_path};
        file << rfl::json::write(config, rfl::json::pretty) << std::endl;
    }

private:
    bool m_config_existed = false;
    fs::path m_path;
};

inline SnpConfig& SnpConfig::instance() {
    static SnpConfig config = SnpConfigLoader{"CrownLink.json"}.load();
    return config;
}