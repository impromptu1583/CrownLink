
#include "DirectIP.h"

#include "Output.h"
#include "UDPSocket.h"
#include "SettingsDialog.h"
#include <juice.h>



#ifdef _WIN32
#include <windows.h>
static void sleep(unsigned int secs) { Sleep(secs * 1000); }
#else
#include <unistd.h> // for sleep
#endif

#define BUFFER_SIZE 4096
static void on_state_changed1(juice_agent_t* agent, juice_state_t state, void* user_ptr);
static void on_state_changed2(juice_agent_t* agent, juice_state_t state, void* user_ptr);

0static void on_candidate1(juice_agent_t* agent, const char* sdp, void* user_ptr);
static void on_candidate2(juice_agent_t* agent, const char* sdp, void* user_ptr);

static void on_gathering_done1(juice_agent_t* agent, void* user_ptr);
static void on_gathering_done2(juice_agent_t* agent, void* user_ptr);

static void on_recv1(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);
static void on_recv2(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);








namespace DRIP
{
  char nName[] = "Direct IP";
  char nDesc[] = "";
  static juice_agent_t* agent1;
  static juice_agent_t* agent2;



  // Agent 1: on state changed
  static void on_state_changed1(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
      printf("State 1: %s\n", juice_state_to_string(state));

      if (state == JUICE_STATE_CONNECTED) {
          // Agent 1: on connected, send a message
          const char* message = "Hello from 1";
          juice_send(agent, message, strlen(message));
      }
  }

  // Agent 2: on state changed
  static void on_state_changed2(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
      printf("State 2: %s\n", juice_state_to_string(state));
      if (state == JUICE_STATE_CONNECTED) {
          // Agent 2: on connected, send a message
          const char* message = "Hello from 2";
          juice_send(agent, message, strlen(message));
      }
  }

  // Agent 1: on local candidate gathered
  static void on_candidate1(juice_agent_t* agent, const char* sdp, void* user_ptr) {
      printf("Candidate 1: %s\n", sdp);

      // Agent 2: Receive it from agent 1
      juice_add_remote_candidate(agent2, sdp);
  }

  // Agent 2: on local candidate gathered
  static void on_candidate2(juice_agent_t* agent, const char* sdp, void* user_ptr) {
      printf("Candidate 2: %s\n", sdp);

      // Agent 1: Receive it from agent 2
      juice_add_remote_candidate(agent1, sdp);
  }

  // Agent 1: on local candidates gathering done
  static void on_gathering_done1(juice_agent_t* agent, void* user_ptr) {
      printf("Gathering done 1\n");
      juice_set_remote_gathering_done(agent2); // optional
  }

  // Agent 2: on local candidates gathering done
  static void on_gathering_done2(juice_agent_t* agent, void* user_ptr) {
      printf("Gathering done 2\n");
      juice_set_remote_gathering_done(agent1); // optional
  }

  // Agent 1: on message received
  static void on_recv1(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
      char buffer[BUFFER_SIZE];
      if (size > BUFFER_SIZE - 1)
          size = BUFFER_SIZE - 1;
      memcpy(buffer, data, size);
      buffer[size] = '\0';
      printf("Received 1: %s\n", buffer);
  }

  // Agent 2: on message received
  static void on_recv2(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
      char buffer[BUFFER_SIZE];
      if (size > BUFFER_SIZE - 1)
          size = BUFFER_SIZE - 1;
      memcpy(buffer, data, size);
      buffer[size] = '\0';
      printf("Received 2: %s\n", buffer);
  }


  SNP::NetworkInfo networkInfo = { nName, 'DRIP', nDesc,
    // CAPS:
  {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

  UDPSocket session;

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
   
    juice_set_log_level(JUICE_LOG_LEVEL_DEBUG);
    // Agent 1: Create agent
    juice_config_t config1;
    memset(&config1, 0, sizeof(config1));

    // STUN server example
    config1.stun_server_host = "stun.l.google.com";
    config1.stun_server_port = 19302;

    config1.cb_state_changed = on_state_changed1;
    config1.cb_candidate = on_candidate1;
    config1.cb_gathering_done = on_gathering_done1;
    config1.cb_recv = on_recv1;
    config1.user_ptr = NULL;

    // Agent 2: Create agent
    juice_config_t config2;
    memset(&config2, 0, sizeof(config2));

    // STUN server example
    config2.stun_server_host = "stun.l.google.com";
    config2.stun_server_port = 19302;
    config2.cb_state_changed = on_state_changed2;
    config2.cb_candidate = on_candidate2;
    config2.cb_gathering_done = on_gathering_done2;
    config2.cb_recv = on_recv2;
    config2.user_ptr = NULL;

    agent1 = juice_create(&config1);
    agent2 = juice_create(&config2);

    // Agent 1: Generate local description
    char sdp1[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(agent1, sdp1, JUICE_MAX_SDP_STRING_LEN);
    printf("Local description 1:\n%s\n", sdp1);

    // Agent 2: Receive description from agent 1
    juice_set_remote_description(agent2, sdp1);

    // Agent 2: Generate local description
    char sdp2[JUICE_MAX_SDP_STRING_LEN];
    juice_get_local_description(agent2, sdp2, JUICE_MAX_SDP_STRING_LEN);
    printf("Local description 2:\n%s\n", sdp2);

    // Agent 1: Receive description from agent 2
    juice_set_remote_description(agent1, sdp2);

    // Agent 1: Gather candidates (and send them to agent 2)
    juice_gather_candidates(agent1);
    sleep(2);

    // Agent 2: Gather candidates (and send them to agent 1)
    juice_gather_candidates(agent2);
    sleep(2);
    bool success = false;
    juice_state_t state1 = juice_get_state(agent1);
    juice_state_t state2 = juice_get_state(agent2);
    while (!success) {
        state1 = juice_get_state(agent1);
        state2 = juice_get_state(agent2);
        success = (state1 == JUICE_STATE_COMPLETED && state2 == JUICE_STATE_COMPLETED);
        sleep(2);
    }





    char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];




    // bind to port
    rebind();
  }
  void DirectIP::destroy()
  {
    hideSettingsDialog();
    session.release();
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
