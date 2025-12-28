#pragma once
#include <thread>

#include "../CrowServe/CrowServe.h"

class JuiceManager;

class CrowServeManager {
public:
    CrowServeManager();
    ~CrowServeManager();

    CrowServeManager(const CrowServeManager&) = delete;
    CrowServeManager& operator=(const CrowServeManager&) = delete;

    CrowServeManager(const CrowServeManager&&) = delete;
    CrowServeManager& operator=(const CrowServeManager&&) = delete;

    CrowServe::Socket& socket() { return m_socket; };


private:
    void init_listener();

private:
    CrowServe::Socket m_socket;
    std::jthread m_listener_thread;
};