#pragma once

#include "common.h"
#include "SNPNetwork.h"
#include "Output.h"
#include "Util/Types.h"
#include <vector>
#include <thread>
#include <chrono>
#include "ThQueue/ThQueue.h"

namespace JP2P
{
    extern SNP::NetworkInfo networkInfo;

    class JuiceP2P : public SNP::Network<SNETADDR>
    {
    public:
        JuiceP2P(){};
        ~JuiceP2P(){};

        void initialize();
        void destroy();
        void requestAds();
        void receive();
        void sendAsyn(const SNETADDR& to, Util::MemoryFrame packet);
        static void receive_signaling();
        static void update_known_advertisers(std::string& message);
        void startAdvertising(Util::MemoryFrame ad);
        void stopAdvertising();
    };
};
