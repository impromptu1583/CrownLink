#pragma once
#include <concurrentqueue.h>

#include <memory>

#include "../types.h"

class CrowServeManager;
class JuiceManager;
struct GamePacket;

extern std::unique_ptr<CrowServeManager> g_crowserve;
extern std::unique_ptr<JuiceManager> g_juice_manager;
extern std::unique_ptr<moodycamel::ConcurrentQueue<GamePacket>> g_receive_queue;
inline handle g_receive_event;