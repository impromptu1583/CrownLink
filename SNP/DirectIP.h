#pragma once

#include "SNPNetwork.h"

#include "Util/Types.h"
#include "UDPSocket.h"
#include "signaling.h"
#include <string>

namespace DRIP
{
  extern SNP::NetworkInfo networkInfo;

  class DirectIP : public SNP::Network<UDPAddr>
  {
  public:
    DirectIP(){};
    ~DirectIP(){};

    void initialize();
    void destroy();
    void requestAds();
    void sendAsyn(const UDPAddr& to, Util::MemoryFrame packet);
    void receive();
    void receive_signaling();
    void startAdvertising(Util::MemoryFrame ad);
    void stopAdvertising();

    void processIncomingPackets();
    void processIncomingJuicePackets();
  };
};
