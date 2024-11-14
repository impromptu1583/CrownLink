#pragma once
#include "Common.h"

enum class LogLevel {
    None,
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
    Trace
};

struct SnpConfig {
    std::string server = "127.0.0.1";
    std::string port = "33377";
    std::string lobby_password = "";
    bool        add_map_to_lobby_name = true;

    LogLevel log_level = LogLevel::Debug;

    CrownLinkMode mode = CrownLinkMode::CLNK;

    static SnpConfig& instance();
};

class SnpConfigLoader {
public:
    SnpConfigLoader(const std::string& filename) : m_path{g_starcraft_dir / filename} {}

    SnpConfig load() {
        std::ifstream file{m_path};
        Json          json;
        if (file.good()) {
            try {
                json = Json::parse(file);
                m_config_existed = true;
            } catch (const Json::parse_error& e) {
                spdlog::dump_backtrace();
                spdlog::error(
                    "Config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte
                );
            }
        }
        if (m_config_existed) {
            spdlog::info("Config file loaded, contents: {}", json.dump());
        } else {
            spdlog::warn("Config file not found, defaults will be used");
        }

        SnpConfig config{};
        load_field(json, "server", config.server);
        load_field(json, "port", config.port);
        load_field(json, "lobby_password", config.lobby_password);
        load_field(json, "add_map_to_lobby_name", config.add_map_to_lobby_name);

        load_field(json, "log_level", config.log_level);
        switch (config.log_level) {
            case LogLevel::Trace: {
                spdlog::set_level(spdlog::level::trace);
            } break;
            case LogLevel::Debug: {
                spdlog::set_level(spdlog::level::debug);
            } break;
            case LogLevel::Info: {
                spdlog::set_level(spdlog::level::info);
            } break;
            case LogLevel::Warn: {
                spdlog::set_level(spdlog::level::warn);
            } break;
            case LogLevel::Error: {
                spdlog::set_level(spdlog::level::err);
            } break;
            case LogLevel::Fatal: {
                spdlog::set_level(spdlog::level::critical);
            } break;
            case LogLevel::None: {
                spdlog::set_level(spdlog::level::off);
            } break;
        }

        save(config);
        return config;
    }

    void save(const SnpConfig& config) {
        if (m_path.empty()) {
            return;
        }

        const auto json = Json{
            {"server", config.server},
            {"port", config.port},
            {"lobby_password", config.lobby_password},
            {"add_map_to_lobby_name", config.add_map_to_lobby_name},
            {"log_level", config.log_level},
        };

        if (m_config_existed) {
            spdlog::info("Creating new config at {}", m_path.string());
        } else {
            spdlog::info("Updating config at {}", m_path.string());
        }

        std::ofstream file{m_path};
        file << std::setw(4) << json << std::endl;
    }

private:
    Json* section(Json& json, const std::string& key) {
        return json.is_object() && json.contains(key) ? &json[key] : nullptr;
    }

    template <typename T>
    void load_field(Json& json, const std::string& key, T& out_value) {
        try {
            json.at(key).get_to(out_value);
        } catch (const Json::out_of_range& ex) {
            spdlog::warn("Value for \"{}\" not found (using default), exception: {}", key, ex.what());
        } catch (const Json::type_error& ex) {
            spdlog::warn("Value for \"{}\" is of wrong type (using default), exception: {}", key, ex.what());
        }
    }

private:
    bool     m_config_existed = false;
    fs::path m_path;
};

inline SnpConfig& SnpConfig::instance() {
    static SnpConfig config = SnpConfigLoader{"CrownLink.json"}.load();
    return config;
}