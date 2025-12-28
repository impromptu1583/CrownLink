#pragma once

#include <concurrentqueue.h>
#include "../NetShared/StormTypes.h"

inline std::unique_ptr<moodycamel::ConcurrentQueue<GamePacket>> g_receive_queue;
inline HANDLE g_receive_event;