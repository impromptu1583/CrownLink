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
	std::string server = "crownlink.platypus.coffee";
	u16 port = 9988;

	std::string stun_server = "stun.l.google.com";
	u16 stun_port = 19302;

	std::string turn_server = "global.relay.metered.ca";
	u16 turn_port = 80;
	std::string turn_username = "";
	std::string turn_password = "";

	LogLevel log_level = LogLevel::Debug;

	static SnpConfig& instance();
};

class SnpConfigLoader {
public:
	SnpConfigLoader(const std::string& filename) : m_path{g_starcraft_dir / filename} {}

	SnpConfig load() {
		std::ifstream file{m_path};
		Json json;
		if (file.good()) {
			try {
				json = Json::parse(file);
				m_config_existed = true;
			} catch (const Json::parse_error& e){
				spdlog::error("Config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte);
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
		
		if (auto stun = section(json, "stun")) {
			load_field(*stun, "server", config.stun_server);
			load_field(*stun, "port", config.stun_port);
		}

		if (auto turn = section(json, "turn")) {
			load_field(*turn, "server", config.turn_server);
			load_field(*turn, "port", config.turn_port);
			load_field(*turn, "username", config.turn_username);
			load_field(*turn, "password", config.turn_password);
		}

		load_field(json, "log-level", config.log_level);
		switch (config.log_level) {
			case LogLevel::Trace: {
				spdlog::set_level(spdlog::level::trace);
			}break;
			case LogLevel::Debug: {
				spdlog::set_level(spdlog::level::debug);
			}break;
			case LogLevel::Info: {
				spdlog::set_level(spdlog::level::info);
			}break;
			case LogLevel::Warn: {
				spdlog::set_level(spdlog::level::warn);
			}break;
			case LogLevel::Error: {
				spdlog::set_level(spdlog::level::err);
			}break;
			case LogLevel::Fatal: {
				spdlog::set_level(spdlog::level::critical);
			}break;
			case LogLevel::None:{
				spdlog::set_level(spdlog::level::off);
			}break;
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
			{"stun", {
				{"server", config.stun_server},
				{"port", config.stun_port},
			}},
			{"turn", {
				{"server", config.turn_server},
				{"port", config.turn_port},
				{"username", config.turn_username},
				{"password", config.turn_password},
			}},
			{"log-level", config.log_level},
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
			spdlog::warn("Value for \"{}\" not found (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		} catch (const Json::type_error& ex) {
			spdlog::warn("Value for \"{}\" is of wrong type (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		}
	}

private:
	bool m_config_existed = false;
	fs::path m_path;
};

inline SnpConfig& SnpConfig::instance() {
	static SnpConfig config = SnpConfigLoader{"CrownLink.json"}.load();
	return config;
}