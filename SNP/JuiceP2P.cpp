
#include "JuiceP2P.h"


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

LogFile trace_file{ "trace" };
Logger log_trace(&trace_file, "Trace");
ThQueue<GamePacket> receive_queue;


namespace JP2P
{

    char nName[] = "Juice P2P";
    char nDesc[] = "";
    
    struct AdFile
    {
        game gameInfo;
        char extraBytes[32];
    };

    SNP::NetworkInfo networkInfo = { nName, 'JP2P', nDesc,
    // CAPS:
    {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

    signaling::SignalingSocket signaling_socket;
    JuiceMAN juice_manager(signaling_socket);
    
    // signaling server provides list of known advertisers
    std::vector<SNETADDR> m_known_advertisers;
    
    // ----------------- game list section -----------------------
    Util::MemoryFrame adData;
    bool isAdvertising = false;

    void JuiceP2P::initialize()
    {
        std::jthread signal_thread(receive_signaling);
        signal_thread.detach();
    }
    void JuiceP2P::destroy()
    {
    }
    void JuiceP2P::requestAds()
    {
        signaling_socket.request_advertisers();
        for ( auto& advertiser : m_known_advertisers) {
            if (juice_manager.peer_status(advertiser) == JUICE_STATE_CONNECTED
                || juice_manager.peer_status(advertiser) == JUICE_STATE_COMPLETED)
            {
                signaling_socket.send_packet(advertiser, signaling::SIGNAL_SOLICIT_ADS);
            }
        }
    }
    void JuiceP2P::receive() {};//unused
    void JuiceP2P::sendAsyn(const SNETADDR& peer_ID, Util::MemoryFrame packet)
    {
        // create header
        char sendBufferBytes[600];
        Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
        Util::MemoryFrame spacket = sendBuffer;
        spacket.write(packet);

        // send packet
        std::string peer_str((char*)peer_ID.address, sizeof(SNETADDR));
        juice_manager.send_p2p(peer_str, packet);
    }

    void JuiceP2P::receive_signaling()
    {
        std::vector<signaling::Signal_packet> incoming_packets;

        char sendBufferBytes[600];
        Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
        Util::MemoryFrame ad_packet = sendBuffer;
        AdFile ad;
        std::string decoded_data;

        // should block here now
        while (true) {
            incoming_packets = signaling_socket.receive_packets();
            log_trace.debug("received incoming signaling");
            for (auto packet : incoming_packets)
            {
                switch (packet.message_type)
                {
                case signaling::SIGNAL_START_ADVERTISING:
                    break;
                case signaling::SIGNAL_STOP_ADVERTISING:
                    break;
                case signaling::SIGNAL_REQUEST_ADVERTISERS:
                    // list of advertisers returned
                    // split into individual addresses & create juice peers
                    update_known_advertisers(packet.data);
                    break;
                case signaling::SIGNAL_SOLICIT_ADS:
                    if (isAdvertising)
                    {
                        std::string send_buffer;
                        send_buffer.append((char*)adData.begin(), adData.size());
                        signaling_socket.send_packet(packet.peer_ID, signaling::SIGNAL_GAME_AD,
                            base64::to_base64(send_buffer));
                    }
                    break;
                case signaling::SIGNAL_GAME_AD:
                    // -------------- PACKET: GAME STATS -------------------------------
                    // give the ad to storm
                    decoded_data = base64::from_base64(packet.data);
                    memcpy(&ad, decoded_data.c_str(), decoded_data.size());
                    SNP::passAdvertisement(packet.peer_ID, Util::MemoryFrame::from(ad));
                    break;
                case signaling::SIGNAL_JUICE_LOCAL_DESCRIPTION:
                case signaling::SIGNAL_JUICE_CANDIDATE:
                case signaling::SIGNAL_JUICE_DONE:
                    juice_manager.signal_handler(packet);
                    break;
                }
            }
        }
    }
    void JuiceP2P::update_known_advertisers(std::string& data)
    {
        std::string data_decoded = base64::from_base64(data);
        m_known_advertisers.clear();
        for (size_t i = 0; (i + 1) * sizeof(SNETADDR) <= data_decoded.size(); i++)
        {
            std::string peer_id = data_decoded.substr(i, sizeof(SNETADDR));
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
