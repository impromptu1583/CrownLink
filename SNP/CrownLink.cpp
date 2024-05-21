
#include "CrownLink.h"

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

namespace CLNK
{
    
    char nName[] = "CrownLink";
    char nDesc[] = "";
    
    struct AdFile
    {
        game gameInfo;
        char extraBytes[32] = "";
    };

    SNP::NetworkInfo networkInfo = { nName, 'CLNK', nDesc,
    // CAPS:
    {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

   
    // signaling server provides list of known advertisers
    std::vector<SNETADDR> m_known_advertisers;
    auto adData = Util::MemoryFrame();
    auto isAdvertising = false;
    std::stop_source stop_source;

    void JuiceP2P::initialize()
    {
        g_logger.info("Initializing, version {}", CL_VERSION);
        signaling_socket.initialize();
        std::jthread signal_thread(receive_signaling);
        signal_thread.detach();
    }
    void JuiceP2P::destroy()
    {   
        g_logger.info("Shutting down");
        //TODO cleanup properly so we don't get an error on close
    }
    void JuiceP2P::requestAds()
    {
        g_logger.debug("Requesting lobbies");
        signaling_socket.request_advertisers();
        for ( auto& advertiser : m_known_advertisers) {
            if (juice_manager.peer_status(advertiser) == JUICE_STATE_CONNECTED
                || juice_manager.peer_status(advertiser) == JUICE_STATE_COMPLETED)
            {
                g_logger.trace("Requesting game state from {}", base64::to_base64(std::string((char*)advertiser.address,sizeof(SNETADDR))));
                signaling_socket.send_packet(advertiser, SIGNAL_SOLICIT_ADS);
            }
        }
    }
    void JuiceP2P::receive() {};//unused in this connection type
    void JuiceP2P::sendAsyn(const SNETADDR& peer_ID, Util::MemoryFrame packet){
        juice_manager.send_p2p(
            std::string((char*)peer_ID.address,sizeof(SNETADDR)), packet);
    }

    void JuiceP2P::receive_signaling(){
        std::vector<Signal_packet> incoming_packets;

        AdFile ad;
        std::string decoded_data;
        auto teststr = std::string("test change game name");

        while (true) {
            signaling_socket.receive_packets(incoming_packets);
            for (auto packet : incoming_packets)
            {
                switch (packet.message_type)
                {
                case SIGNAL_START_ADVERTISING:
                    g_logger.debug("server confirmed lobby open");
                    break;
                case SIGNAL_STOP_ADVERTISING:
                    g_logger.debug("server confirmed lobby closed");
                    break;
                case SIGNAL_REQUEST_ADVERTISERS:
                    // list of advertisers returned
                    // split into individual addresses & create juice peers
                    update_known_advertisers(packet.data);
                    break;
                case SIGNAL_SOLICIT_ADS:
                    if (isAdvertising)
                    {
                        g_logger.debug("received solicitation from {}, replying with our lobby info",packet.peer_ID.b64());
                        std::string send_buffer;
                        send_buffer.append((char*)adData.begin(), adData.size());
                        signaling_socket.send_packet(packet.peer_ID, SIGNAL_GAME_AD,
                            base64::to_base64(send_buffer));
                    }
                    break;
                case SIGNAL_GAME_AD:
                    // -------------- PACKET: GAME STATS -------------------------------
                    // give the ad to storm
                    g_logger.debug("received lobby info from {}", packet.peer_ID.b64());
                    decoded_data = base64::from_base64(packet.data);
                    memcpy(&ad, decoded_data.c_str(), decoded_data.size());
                    //auto teststr = std::string("test change game name");
                    //memcpy(ad.gameInfo.szGameName, teststr.c_str(), teststr.size());
                    SNP::passAdvertisement(packet.peer_ID, Util::MemoryFrame::from(ad));

                    g_logger.debug("Game Info Received:\n"
                        "  dwIndex: {}\n"
                        "  dwGameState: {}\n"
                        "  saHost: {}\n"
                        "  dwTimer: {}\n"
                        "  szGameName[128]: {}\n"
                        "  szGameStatString[128]: {}\n"
                        "  dwExtraBytes: {}\n"
                        "  dwProduct: {}\n"
                        "  dwVersion: {}\n",
                        ad.gameInfo.dwIndex,
                        ad.gameInfo.dwGameState,
                        ad.gameInfo.saHost.b64(),
                        ad.gameInfo.dwTimer,
                        ad.gameInfo.szGameName,
                        ad.gameInfo.szGameStatString,
                        ad.gameInfo.dwExtraBytes,
                        ad.gameInfo.dwProduct,
                        ad.gameInfo.dwVersion
                    );
                    


                    break;
                case SIGNAL_JUICE_LOCAL_DESCRIPTION:
                case SIGNAL_JUICE_CANDIDATE:
                case SIGNAL_JUICE_DONE:
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
            g_logger.trace("[update_known_advertisers] potential lobby owner received: {}",base64::to_base64(peer_id));
            m_known_advertisers.push_back(SNETADDR(peer_id));
            juice_manager.create_if_not_exist(peer_id);
        }
    }
    void JuiceP2P::startAdvertising(Util::MemoryFrame ad)
    {
        adData = ad;
        isAdvertising = true;
        signaling_socket.start_advertising();
        g_logger.info("started advertising lobby");
    }
    void JuiceP2P::stopAdvertising()
    {
        isAdvertising = false;
        signaling_socket.stop_advertising();
        g_logger.info("stopped advertising lobby");
    }
    //------------------------------------------------------------------------------------------------

};
