
#include "JuiceP2P.h"


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

namespace JP2P
{
    char nName[] = "Juice P2P";
    char nDesc[] = "";


    SNP::NetworkInfo networkInfo = { nName, 'JP2P', nDesc,
    // CAPS:
    {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

    SignalingSocket signaling_socket;
    moodycamel::ConcurrentQueue<std::string> receive_queue;
    JuiceMAN juice_manager(signaling_socket,&receive_queue);
    std::vector<SNETADDR> m_known_advertisers;

    // ----------------- game list section -----------------------
    Util::MemoryFrame adData;
    bool isAdvertising = false;
    //SIGNALING need

    // --------------   incoming section  ----------------------
    char recvBufferBytes[1024];

    // ---------------  packet IDs  ------------------------------
    //const int PacketType_RequestGameStats = 1;
    //const int PacketType_GameStats = 2;
    //const int PacketType_GamePacket = 3;
    enum PacketType
    {
        PacketType_Solicitation = 1,
        PacketType_Advertisement,
        PacketType_GamePacket
    };

    //------------------------------------------------------------------------------------------------------------------------------------

    void JuiceP2P::processIncomingPackets()
    {
        std::string incoming_packet;
        while (receive_queue.try_dequeue(incoming_packet)) {
            SNETADDR peer_ID;
            std::string peer_str = incoming_packet.substr(0, 16);
            memcpy(&peer_ID, peer_str.c_str(), sizeof(SNETADDR));
            std::string data = incoming_packet.substr(16);
            Util::MemoryFrame packet((void*)data.c_str(), data.size());
            int type = packet.readAs<int>();
            if (type == PacketType(PacketType_Solicitation))
            {
                // -------------- PACKET: REQUEST GAME STATES -----------------------
                if (isAdvertising)
                {
                    // if state is connected
                    // send game ad through signaling
                    // send back game stats
                    char sendBufferBytes[600];
                    Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
                    Util::MemoryFrame spacket = sendBuffer;
                    spacket.writeAs<int>(PacketType(PacketType_Advertisement));
                    spacket.write(adData);
                    juice_manager.send_p2p(peer_str,
                        sendBuffer.getFrameUpto(spacket));
                }
            }
            else if (type == PacketType(PacketType_Advertisement))
            {
                // -------------- PACKET: GAME STATS -------------------------------
                // give the ad to storm
                passAdvertisement(peer_ID, packet);
            }
            else if (type == PacketType(PacketType_GamePacket))
            {
                // -------------- PACKET: GAME PACKET ------------------------------
                // pass strom packet to strom
                passPacket(peer_ID, packet);
            }
        }
    }
    //------------------------------------------------------------------------------------------------------------------------------------
    void JuiceP2P::initialize()
    {
    }
    void JuiceP2P::destroy()
    {
    }
    void JuiceP2P::requestAds()
    {
        receive();

        // send game state request
        char sendBufferBytes[600];
        Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
        Util::MemoryFrame ping_server = sendBuffer;
        ping_server.writeAs<int>(PacketType(PacketType_Solicitation));

        //JUICE
        signaling_socket.request_advertisers();
        juice_manager.send_all(sendBuffer.getFrameUpto(ping_server));
        // new behavior
        for ( auto& advertiser : m_known_advertisers) {
            if (juice_manager.peer_status(advertiser) == JUICE_STATE_CONNECTED
                || juice_manager.peer_status(advertiser) == JUICE_STATE_COMPLETED)
            {
                signaling_socket.send_packet_type(advertiser, SERVER_SOLICIT_ADS);
            }
        }
    }
    void JuiceP2P::sendAsyn(const SNETADDR& peer_ID, Util::MemoryFrame packet)
    {
        receive();
        // create header
        char sendBufferBytes[600];
        Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
        Util::MemoryFrame spacket = sendBuffer;
        spacket.writeAs<int>(PacketType(PacketType_GamePacket));
        spacket.write(packet);

        // send packet
        std::string peer_str;
        peer_str.append((char*)peer_ID.address, sizeof(SNETADDR));

        juice_manager.send_p2p(peer_str, sendBuffer.getFrameUpto(spacket));
    }
    void JuiceP2P::receive()
    {
    receive_signaling();
    processIncomingPackets();
    }
    void JuiceP2P::receive_signaling()
    {
        std::vector<Signal_packet> incoming_packets;

        incoming_packets = signaling_socket.receive_packets();
        for (auto packet : incoming_packets)
        {
            if (packet.data.empty()) continue;
            //std::string from_address = packet.substr(0, 16);
            //std::string received_message = packet.substr(16);
            if (signaling_socket.server.compare(packet.peer_ID) == 0)
            {
                // communication from server, not peer
                switch (packet.message_type)
                {
                case SERVER_START_ADVERTISING:
                    // confirmed advertising
                    break;
                case SERVER_STOP_ADVERTISING:
                    // confirmed stop advertising
                    break;
                case SERVER_REQUEST_ADVERTISERS:
                    // list of advertisers returned
                    // split into individual addresses & create juice peers
                    update_known_advertisers(packet.data);
                    break;
                }
            }
            else
            {
                // message is relayed from a peer
                Util::MemoryFrame ad_packet((void*)packet.data.c_str(), packet.data.size());
                switch (packet.message_type)
                {
                case SERVER_SOLICIT_ADS:
                    if (isAdvertising)
                    {
                        std::string send_buffer;
                        send_buffer.append((char*)adData.begin(), adData.size());
                        signaling_socket.send_packet_type(packet.peer_ID, SERVER_GAME_AD, send_buffer);
                    }
                    break;
                case SERVER_GAME_AD:
                    // -------------- PACKET: GAME STATS -------------------------------
                    // give the ad to storm
                    
                    passAdvertisement(packet.peer_ID, ad_packet);
                    break;
                case SERVER_RELAY_TO_PEER:
                    break;
                }
                juice_manager.signal_handler(packet);
            }
        }
    }
    void JuiceP2P::update_known_advertisers(std::string& received_message)
    {
        m_known_advertisers.clear();
        for (size_t i = 0; (i + 1) * ADDRESS_SIZE + 1 <= received_message.size(); i++)
        {
            std::string peer_id = received_message.substr(i + 1, ADDRESS_SIZE);
            m_known_advertisers.push_back(SNETADDR(peer_id));
            juice_manager.create_if_not_exist(peer_id);
        }
    }
    void JuiceP2P::startAdvertising(Util::MemoryFrame ad)
    {
        adData = ad;
        isAdvertising = true;

        signaling_socket.start_advertising();
    }
    void JuiceP2P::stopAdvertising()
    {
        isAdvertising = false;
        signaling_socket.stop_advertising();
    }
    //------------------------------------------------------------------------------------------------
};
