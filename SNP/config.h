#pragma once
#include "common.h"

struct SnpConfig {
	std::string server = "crownlink.platypus.coffee";
	u32 port = 9988;
	LogLevel log_level = LogLevel::Debug;
};

class SnpConfigLoader {
public:
	SnpConfigLoader(std::initializer_list<fs::path> paths) {
		for (const auto& path : paths) {
			if (m_save_path.empty()) {
				m_save_path = path;
			}

			std::ifstream file{path};
			if (!file.good()) {
				continue;
			}

			try {
				m_json = json::parse(file);
				m_save_path = path;
				m_config_existed = true;
				break;
			} catch (const json::parse_error& e){
				m_logger.error("config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte);
			}
		}
		if (m_config_existed) {
			m_logger.info("logfile loaded, contents: {}", m_json.dump());
		} else {
			m_logger.warn("config file not found, defaults will be used");
		}
	}

	SnpConfig load() {
		SnpConfig config;
		load_field("server", config.server);
		load_field("port", config.port);
		load_field("log-level", config.log_level);

		Logger::set_log_level(config.log_level);

		save(config);
		return config;
	}

	void save(const SnpConfig& config) {
		if (m_save_path.empty()) {
			return;
		}

		json json_{
			{"server", config.server},
			{"port", config.port},
			{"log-level", config.log_level},
		};

		if (m_config_existed) {
			m_logger.info("Creating new config at {}", m_save_path.string());
		} else {
			m_logger.info("Updating config at {}", m_save_path.string());
		}

		std::ofstream file{m_save_path};
		file << std::setw(4) << json_;
	}

private:
	template <typename T>
	void load_field(const std::string& key, T& out_value) {
		try {
			m_json.at(key).get_to(out_value);
		} catch (const json::out_of_range& ex) {
			m_logger.warn("config value for \"{}\" not found (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		} catch (const json::type_error& ex) {
			m_logger.warn("config value for \"{}\" is of wrong type (using default: {}), exception: {}", key, as_string(out_value), ex.what());
		}
	}

private:
	bool m_config_existed = false;
	fs::path m_save_path;
	json m_json;
	Logger m_logger{g_root_logger, "Config"};
};

inline SnpConfig g_snp_config = SnpConfigLoader{"CrownLink_config.json", "..\\Starcraft\\CrownLink_config.json", "C:\\Cosmonarchy\\Starcraft\\CrownLink_config.json"}.load();