#pragma once

#include "Util/MemoryFrame.h"
#include "Util/Types.h"
#include "Storm/storm.h"

//
// The Network interface separates the Storm stuff from pure networking
//

namespace SNP
{
  const int PACKET_SIZE = 500;

  struct NetworkInfo
  {
    char          *pszName;
    DWORD         dwIdentifier;
    char          *pszDescription;
    CAPS          caps;
  };

  typedef ::SOCKADDR SOCKADDR;
  typedef ::SNETADDR SNETADDR;

  extern void passAdvertisement(const SNETADDR& host, Util::MemoryFrame ad);
  extern void removeAdvertisement(const SNETADDR& host);
  extern void passPacket(const SNETADDR& host, Util::MemoryFrame packet);

  template<typename PEERID>
  class Network
  {
  public:
    Network()
    {
    }
    virtual ~Network()
    {
    }

    SNETADDR makeBin(const PEERID& src)
    {
        SNETADDR retval;
        memcpy_s(&retval, sizeof(SNETADDR), &src, sizeof(PEERID));
        memset(((BYTE*)&retval)+sizeof(PEERID), 0, sizeof(SNETADDR) - sizeof(PEERID));
        return retval;
    }

    // callback functions that take network specific arguments and cast them away
    void passAdvertisement(const PEERID& host, Util::MemoryFrame ad)
    {
      SNP::passAdvertisement(makeBin(host), ad);
    }
    void removeAdvertisement(const PEERID& host)
    {
      SNP::removeAdvertisement(makeBin(host));
    }
    void passPacket(const PEERID& host, Util::MemoryFrame packet)
    {
      SNP::passPacket(makeBin(host), packet);
    }

    // network plug functions
    virtual void initialize() = 0;
    virtual void destroy() = 0;
    virtual void requestAds() = 0;
    virtual void sendAsyn(const PEERID& to, Util::MemoryFrame packet) = 0;
    virtual void receive() = 0;
    virtual void startAdvertising(Util::MemoryFrame ad) = 0;
    virtual void stopAdvertising() = 0;
  };

  typedef Network<SNETADDR> BinNetwork;
}
