#include <winsock2.h>

#include "SNPModule.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "CrownLink.h"
#define CLNK_ID 0

BOOL WINAPI SnpQuery(DWORD dwIndex, DWORD* dwNetworkCode, char** ppszNetworkName, char** ppszNetworkDescription, CAPS** ppCaps) {
	if (dwNetworkCode && ppszNetworkName && ppszNetworkDescription && ppCaps) {
		switch (dwIndex) {
			case CLNK_ID:
				*dwNetworkCode = clnk::g_network_info.dwIdentifier;
				*ppszNetworkName = clnk::g_network_info.pszName;
				*ppszNetworkDescription = clnk::g_network_info.pszDescription;
				*ppCaps = &clnk::g_network_info.caps;
				return TRUE;
			default:
				return FALSE;
		}
	}
	return FALSE;
}

BOOL WINAPI SnpBind(DWORD dwIndex, snp::NetFunctions** ppFxns) {
	if (ppFxns) {
		switch (dwIndex) {
			case CLNK_ID:
				*ppFxns = &snp::g_spi_functions;
				snp::g_plugged_network = std::make_unique<clnk::CrownLink>();
				return TRUE;
			default:
				return FALSE;
		}
	}
	return FALSE;
}

HINSTANCE hInstance;

static void dll_start() {
	// init winsock
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;

	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		g_root_logger.fatal("WSAStartup failed with error {}", err);
	}
}

static void dll_exit() {
	WSACleanup();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
		{
			hInstance = hinstDLL;

			dll_start();
			std::atexit(dll_exit);
		} break;
	}
	return TRUE;
}
