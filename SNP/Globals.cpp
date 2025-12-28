#include "Globals.h"
#include "CrowServeManager.h"
#include "JuiceManager.h"

std::unique_ptr<CrowServeManager> g_crowserve;
std::unique_ptr<JuiceManager> g_juice_manager;
std::unique_ptr<moodycamel::ConcurrentQueue<GamePacket>> g_receive_queue;