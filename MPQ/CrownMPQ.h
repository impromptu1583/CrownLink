#include "../shared_common.h"

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#define STORMLIB_NO_AUTO_LINK
#include "StormLib.h"
#include <filesystem>
namespace fs = std::filesystem;

using u32 = unsigned int;

struct CAPS {
    u32 size;
    u32 flags;             // 0x20000003. 0x00000001 = page locked buffers, 0x00000002 = basic interface, 0x20000000 = release mode
    u32 max_message_size;  // must be between 128 and 504
    u32 max_queue_size;
    u32 max_players;
    u32 bytes_per_second;
    u32 latency;
    u32 turns_per_second; // the turn_per_second and turns_in_transit values
    u32 turns_in_transit; // in the appended mpq file seem to be used instead
};

struct DAT {
    DAT(std::string id, std::string name, std::string requirements, CAPS caps)
        :m_id(id), m_name(name), m_requirements(requirements), m_caps(caps) {
        std::reverse(m_id.begin(), m_id.end());
        m_size = 4 + name.size() + 1 + requirements.size() + 1 + sizeof(caps);
    }
    u32 m_size; // not included in size
    std::string m_id;
    std::string m_name;
    std::string m_requirements;
    CAPS m_caps;
    void write_file(std::string filename) const {
        std::fstream s{filename, std::ios::out | std::ios::binary | std::ios::app};
        s.write(reinterpret_cast<const char*>(&m_size), sizeof m_size);
        s.write(m_id.c_str(), 4);
        s.write(m_name.c_str(), m_name.size() + 1);
        s.write(m_requirements.c_str(), m_requirements.size() + 1);
        s.write((char*) &m_caps, sizeof(m_caps));
        s.close();
    }
    void get_dat(std::stringstream& s) {
        s.write(reinterpret_cast<const char*>(&m_size), sizeof m_size);
        s.write(m_id.c_str(), 4);
        s.write(m_name.c_str(), m_name.size() + 1);
        s.write(m_requirements.c_str(), m_requirements.size() + 1);
        s.write((char*) &m_caps, sizeof(m_caps));
    }
};