#pragma once

#include "SNPNetwork.h"

#include "Output.h"

#include "Util/Types.h"
#include "signaling.h"
#include "JuiceManager.h"
#include <string>
#include <vector>

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
        void sendAsyn(const SNETADDR& to, Util::MemoryFrame packet);
        void receive();
        void receive_signaling();
        void update_known_advertisers(std::string& message);
        void startAdvertising(Util::MemoryFrame ad);
        void stopAdvertising();
        void processIncomingPackets();
    };
};
