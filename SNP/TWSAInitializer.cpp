#include "TWSAInitializer.h"

TWSAInitializer::TWSAInitializer()
{
    WSADATA WsaDat;
    if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
        throw GeneralException("WSA initialization failed");
    // TWSAInitializer::completion_port = CreateIoCompletionPort(NULL, NULL, NULL, 0);
}

TWSAInitializer::~TWSAInitializer()
{
    WSACleanup();
}

TWSAInitializer _init_wsa;