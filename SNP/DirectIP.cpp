
#include "DirectIP.h"

#include "Output.h"
#include "UDPSocket.h"
#include "SettingsDialog.h"
#include <juice.h>
#include <windows.h>
static void sleep(unsigned int secs) { Sleep(secs * 1000); }
#define BUFFER_SIZE 4096


static void copy_clipboard(const char* output) {
    const size_t len = strlen(output) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), output, len);
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

namespace DRIP
{
  char nName[] = "Direct IP";
  char nDesc[] = "";


  SNP::NetworkInfo networkInfo = { nName, 'DRIP', nDesc,
    // CAPS:
  {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

  UDPSocket session;
  UDPSocket signalSession;


  // ----------------- game list section -----------------------
  Util::MemoryFrame adData;
  bool isAdvertising = false;

  // --------------   incoming section  ----------------------
  char recvBufferBytes[1024];

  // ---------------  packet IDs  ------------------------------
  const int PacketType_RequestGameStats = 1;
  const int PacketType_GameStats = 2;
  const int PacketType_GamePacket = 3;

  //------------------------------------------------------------------------------------------------------------------------------------
  // JUICE
  static juice_agent_t* agent;
  static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
  static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
  static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
  static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

  //------------------------------------------------------------------------------------------------------------------------------------
  static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
      printf("State 1: %s\n", juice_state_to_string(state));

      if (state == JUICE_STATE_CONNECTED) {
          // Agent 1: on connected, send a message
          const char* message = "Hello from 1";
          juice_send(agent, message, strlen(message));
      }
  }
  static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr) {
      printf("Candidate 1: %s\n", sdp);

      // Agent 2: Receive it from agent 1
      // TODO juice_add_remote_candidate(agent2, sdp);
  }
  static void on_gathering_done(juice_agent_t* agent, void* user_ptr) {
      printf("Gathering done 1\n");
      // TODO juice_set_remote_gathering_done(agent2); // optional
  }
  static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
      char buffer[BUFFER_SIZE];
      if (size > BUFFER_SIZE - 1)
          size = BUFFER_SIZE - 1;
      memcpy(buffer, data, size);
      buffer[size] = '\0';
      printf("Received 1: %s\n", buffer);
  }
  //------------------------------------------------------------------------------------------------------------------------------------
  void rebind()
  {
    int targetPort = atoi(getLocalPortString());
    if(session.getBoundPort() == targetPort)
      return;
    try
    {
      session.release();
      session.init();
      session.setBlockingMode(false);
      session.bind(targetPort);
      setStatusString("network ready");
    }
    catch(...)
    {
      setStatusString("local port fail");
    }
  }
  void DirectIP::processIncomingPackets()
  {
    try
    {
      // receive all packets
      while(true)
      {
        // receive next packet
        UDPAddr sender;
        Util::MemoryFrame packet = session.receivePacket(sender, Util::MemoryFrame(recvBufferBytes, sizeof(recvBufferBytes)));
        if(packet.isEmpty())
        {
          if(session.getState() == WSAECONNRESET)
          {
//            DropMessage(1, "target host not reachable");
            setStatusString("host IP not reachable");
            continue;
          }
          if(session.getState() == WSAEWOULDBLOCK)
            break;
          throw GeneralException("unhandled UDP state");
        }

        memset(sender.sin_zero, 0, sizeof(sender.sin_zero));

        int type = packet.readAs<int>();
        if(type == PacketType_RequestGameStats)
        {
          // -------------- PACKET: REQUEST GAME STATES -----------------------
          if(isAdvertising)
          {
            // send back game stats
            char sendBufferBytes[600];
            Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
            Util::MemoryFrame spacket = sendBuffer;
            spacket.writeAs<int>(PacketType_GameStats);
            spacket.write(adData);
            session.sendPacket(sender, sendBuffer.getFrameUpto(spacket));
          }
        }
        else
        if(type == PacketType_GameStats)
        {
          // -------------- PACKET: GAME STATS -------------------------------
          // give the ad to storm
          passAdvertisement(sender, packet);
        }
        else
        if(type == PacketType_GamePacket)
        {
          // -------------- PACKET: GAME PACKET ------------------------------
          // pass strom packet to strom
          passPacket(sender, packet);
        }
      }
    }
    catch(GeneralException &e)
    {
      DropLastError("processIncomingPackets failed: %s", e.getMessage());
    }
  }
  //------------------------------------------------------------------------------------------------------------------------------------
  //------------------------------------------------------------------------------------------------------------------------------------
  void DirectIP::initialize()
  {
    showSettingsDialog();

    // ---------------  Set up Juice  ------------------------------
    juice_config_t config1;
    memset(&config1, 0, sizeof(config1));
    config1.stun_server_host = "stun.l.google.com";
    config1.stun_server_port = 19302;

    config1.cb_state_changed = on_state_changed;
    config1.cb_candidate = on_candidate;
    config1.cb_gathering_done = on_gathering_done;
    config1.cb_recv = on_recv;
    config1.user_ptr = NULL;

    agent = juice_create(&config1);
    // generate local description
    char sdp1[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(agent, sdp1, JUICE_MAX_SDP_STRING_LEN);

    setStatusString("got description");

    copy_clipboard(sdp1);

    // bind to port
    rebind();

  }
  void DirectIP::destroy()
  {
    hideSettingsDialog();
    session.release();
    signalSession.release();
  }
  void DirectIP::requestAds()
  {
    rebind();
    processIncomingPackets();

    // send game state request
    char sendBufferBytes[600];
    Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
    Util::MemoryFrame ping_server = sendBuffer;
    ping_server.writeAs<int>(PacketType_RequestGameStats);

    UDPAddr host;
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = inet_addr(getHostIPString());
    host.sin_port = htons(atoi(getHostPortString()));
    session.sendPacket(host, sendBuffer.getFrameUpto(ping_server));
  }
  void DirectIP::sendAsyn(const UDPAddr& him, Util::MemoryFrame packet)
  {

    processIncomingPackets();

    // create header
    char sendBufferBytes[600];
    Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
    Util::MemoryFrame spacket = sendBuffer;
    spacket.writeAs<int>(PacketType_GamePacket);
    spacket.write(packet);

    // send packet
    session.sendPacket(him, sendBuffer.getFrameUpto(spacket));
  }
  void DirectIP::receive()
  {
    processIncomingPackets();
  }
  void DirectIP::startAdvertising(Util::MemoryFrame ad)
  {
    rebind();
    adData = ad;
    isAdvertising = true;
  }
  void DirectIP::stopAdvertising()
  {
    isAdvertising = false;
  }
  //------------------------------------------------------------------------------------------------------------------------------------
};
