#pragma once
#include "Common.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "storm/Storm.h"

namespace snp {

constexpr auto MAX_PACKET_SIZE = 500;

struct NetworkInfo {
    char *pszName;
    DWORD dwIdentifier;
    char *pszDescription;
    Caps caps;
};

void update_lobbies(std::vector<AdFile> &updated_list);
void create_status_ad();
void update_status_ad();
void set_status_ad(const std::string &status);
void clear_status_ad();
void packet_parser(const GamePacket *game_packet);

bool try_create_game(u32* playerid);
bool join_game(u32 index, u32 *playerid);

struct NetFunctions {
    // The size of the vtable
    DWORD dwSize;

    // Compares two NetAddress with each other and returns the number of differences in dwResult
    BOOL(__stdcall *spiCompareNetAddresses)(NetAddress *addr1, NetAddress *addr2, DWORD *dwResult);

    // Called when the module is released
    BOOL(__stdcall *spiDestroy)();

    // Called in order to free blocks of packet memory returned in the spiReceive functions
    BOOL(__stdcall *spiFree)(NetAddress *addr, char *data, DWORD databytes);
    BOOL(__stdcall *spiFreeExternalMessage)(NetAddress *addr, char *data, DWORD databytes);

    // Returns info on a specified game
    void *spiGetGameInfo;

    // Returns packet statistics
    void *spiGetPerformanceData;

    // Called when the module is initialized
    BOOL(__stdcall *spiInitialize)
    (ProgramInfo *gameClientInfo, PlayerData *userData, InterfaceData *bnCallbacks, VersionInfo *moduleData, HANDLE hEvent);
    void *spiInitializeDevice;
    void *spiLockDeviceList;

    // Called to prevent the game list from updating so that it can be processed by storm
    void *spiLockGameList;

    // Return received data from a connectionless socket to storm
    BOOL(__stdcall *spiReceive)(NetAddress **addr, char **data, DWORD *databytes);

    // Return received data from a connected socket to storm
    BOOL(__stdcall *spiReceiveExternalMessage)(NetAddress **addr, char **data, DWORD *databytes);

    // Called when a game is selected to query information
    //void *spiSelectGame;
    BOOL(__stdcall *spiSelectGame)
    (DWORD flags, ProgramInfo *client_info, PlayerData *user_info, InterfaceData *callbacks, VersionInfo *module_info, u32* playerid);

    // Sends data over a connectionless socket
    BOOL(__stdcall *spiSend)(DWORD addrCount, NetAddress **addrList, char *buf, DWORD bufLen);

    // Sends data over a connected socket
    void *spiSendExternalMessage;

    // An extended version of spiStartAdvertisingGame
    void *spiStartAdvertisingLadderGame;

    // Called to stop advertising the game
    BOOL(__stdcall *spiStopAdvertisingGame)();
    BOOL(__stdcall *spiUnlockDeviceList)(void *unknownStruct);

    // Called after the game list has been processed and resume updating
    void *spiUnlockGameList;

    // Called to begin advertising a created game to other clients
    BOOL(__stdcall *spiGetLocalPlayerName)
    (char *name_buffer, DWORD name_buffer_size, char *desc_buffer, DWORD desc_buffer_size);
    BOOL(__stdcall *spiReportGameResult)(u32 a, u32 b, u32 c, u32 d, u32 e);
    void *spiCheckDataFile;
    void *spiLeagueCommand;
    void *spiLeagueSendReplayPath;
    void *spiLeagueGetReplayPath;
    BOOL(__stdcall *spiLeagueLogout)(char* unknown);
    BOOL(__stdcall *spiLeagueGetName)(char *pszDest, DWORD dwSize);
};

extern NetFunctions g_spi_functions;

};  // namespace snp
