#pragma once
#include "common.h"

class SNPConfig {
public:
	// can specify multiple potential config file locations in constructor
	// example: auto snpconfig = SNPConfig{ "snp_config.json" , "../starcraft/snp_config.json" };
	SNPConfig(std::initializer_list<std::string> config_file_locations) {
		for (const auto& file_name : config_file_locations) {
			std::ifstream f(file_name);
			if (f.good() && !m_config_found) {
				try {
					std::ifstream f(file_name);
					m_json = json::parse(f);
					m_config_found = true;
				} catch (const json::parse_error& e){
					g_root_logger.error("config file error: {}, exception id: {}, error at byte position: {}", e.what(), e.id, e.byte);
				}
			}
		}
		if (m_config_found) {
			g_root_logger.info("logfile loaded, contents: {}", m_json.dump());
		} else {
			g_root_logger.warn("config file not found, defaults will be used");
		}
		g_root_logger.set_log_level(load_or_default<LogLevel>("log level", LogLevel::Info));
	}

	// example: server_address = snp_config.config_or_default("server","127.0.0.1")
	std::string load_or_default(std::string key, std::string default_value) {
		if (!m_config_found) { return default_value; }
		const auto iter = m_json.find(key);
		if (iter != m_json.end()) {
			if (iter.value().is_number()) {
				return to_string(m_json[key]);
			}
			return iter->get<std::string>();
		} else {
			g_root_logger.warn("config value for {} not found, using default {}",key,default_value);
			return default_value;
		}
	}

	template <typename T>
	T load_or_default(const std::string& key, const T& default_value) {
		if (!m_config_found) return default_value;
		const auto iter = m_json.find(key);
		if (iter != m_json.end()) {
			return iter->get<T>();
		} else {
			g_root_logger.warn("config value for {} not found, using default {}", key, as_string(default_value));
			return default_value;
		}
	}

private:
	json m_json;
	bool m_config_found = false;
};

inline auto g_snp_config = SNPConfig{"CrownLink_config.json", "..\\Starcraft\\CrownLink_config.json", "C:\\Cosmonarchy\\Starcraft\\CrownLink_config.json"};