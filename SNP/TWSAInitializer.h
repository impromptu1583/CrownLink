#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "Util/Exceptions.h"

class TWSAInitializer
{
public:
	TWSAInitializer();
	~TWSAInitializer();

public:
	static HANDLE completion_port;
};

extern TWSAInitializer _init_wsa;