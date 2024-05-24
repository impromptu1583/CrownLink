#pragma once
#include "common.h"

struct SnpConfig {
	std::string server = "crownlink.platypus.coffee";
	u16 port = 9988;

	std::string stun_server = "stun.l.google.com";
	u16 stun_port = 19302;

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
				m_logger.error("Config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte);
			}
		}
		if (m_config_existed) {
			m_logger.info("Config file loaded, contents: {}", json.dump());
		} else {
			m_logger.warn("Config file not found, defaults will be used");
		}

		SnpConfig config{};
		load_field(json, "server", config.server);
		load_field(json, "port", config.port);
		
		if (auto stun = section(json, "stun")) {
			load_field(*stun, "server", config.stun_server);
			load_field(*stun, "port", config.stun_port);
		}

		load_field(json, "log-level", config.log_level);
		Logger::set_log_level(config.log_level);

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
			{"log-level", config.log_level},
		};

		if (m_config_existed) {
			m_logger.info("Creating new config at {}", m_path.string());
		} else {
			m_logger.info("Updating config at {}", m_path.string());
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
			m_logger.warn("Value for \"{}\" not found (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		} catch (const Json::type_error& ex) {
			m_logger.warn("Value for \"{}\" is of wrong type (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		}
	}

private:
	bool m_config_existed = false;
	fs::path m_path;
	Logger m_logger{Logger::root(), "Config"};
};

inline SnpConfig& SnpConfig::instance() {
	static SnpConfig config = SnpConfigLoader{"CrownLink.json"}.load();
	return config;
}