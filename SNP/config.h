#pragma once
#include "common.h"

struct SnpConfig {
	std::string server = "crownlink.platypus.coffee";
	u16 port = 9988;

	std::string stun_server = "stun.l.google.com";
	u16 stun_port = 19302;

	LogLevel log_level = LogLevel::Debug;

};

class SnpConfigLoader {
public:
	SnpConfigLoader(const std::string& filename) {
		char buffer[MAX_PATH];
		GetModuleFileNameA(0, buffer, MAX_PATH);
		const auto path = fs::path{buffer}.parent_path() / filename;
		m_path = path;
	}

	SnpConfig load() {
		std::ifstream file{m_path};
		if (file.good()) {
			try {
				m_json = json::parse(file);
				m_config_existed = true;
			} catch (const json::parse_error& e){
				m_logger.error("Config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte);
			}
		}
		if (m_config_existed) {
			m_logger.info("Logfile loaded, contents: {}", m_json.dump());
		} else {
			m_logger.warn("Config file not found, defaults will be used");
		}

		SnpConfig config;
		load_field(m_json, "server", config.server);
		load_field(m_json, "port", config.port);
		
		auto& stun = m_json["stun"];
		load_field(stun, "server", config.stun_server);
		load_field(stun, "port", config.stun_port);

		load_field(m_json, "log-level", config.log_level);
		Logger::set_log_level(config.log_level);

		save(config);
		return config;
	}

	void save(const SnpConfig& config) {
		if (m_path.empty()) {
			return;
		}

		json json_{
			{"server", config.server},
			{"port", config.port},
			{"stun",
				"server", config.stun_server,
				"port", config.stun_port,
			},
			{"log-level", config.log_level},
		};

		if (m_config_existed) {
			m_logger.info("Creating new config at {}", m_path.string());
		} else {
			m_logger.info("Updating config at {}", m_path.string());
		}

		std::ofstream file{m_path};
		file << std::setw(4) << json_;
	}

private:
	template <typename T>
	void load_field(json& json_, const std::string& key, T& out_value) {
		try {
			json_.at(key).get_to(out_value);
		} catch (const json::out_of_range& ex) {
			m_logger.warn("Value for \"{}\" not found (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		} catch (const json::type_error& ex) {
			m_logger.warn("Value for \"{}\" is of wrong type (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		}
	}

private:
	bool m_config_existed = false;
	fs::path m_path;
	json m_json;
	Logger m_logger{g_root_logger, "Config"};
};

inline SnpConfig g_config = SnpConfigLoader{"CrownLink.json"}.load();