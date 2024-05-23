#include <winsock2.h>

#include "SNPModule.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "CrownLink.h"
#define CLNK_ID 0

BOOL WINAPI SnpQuery(DWORD index, DWORD* out_network_code, char** out_network_name, char** out_network_description, CAPS** out_caps) {
	if (out_network_code && out_network_name && out_network_description && out_caps) {
		switch (index) {
			case CLNK_ID: {
				*out_network_code = clnk::g_network_info.dwIdentifier;
				*out_network_name = clnk::g_network_info.pszName;
				*out_network_description = clnk::g_network_info.pszDescription;
				*out_caps = &clnk::g_network_info.caps;
				return true;
			} break;
		}
	}
	return false;
}

BOOL WINAPI SnpBind(DWORD index, snp::NetFunctions** out_funcs) {
	if (out_funcs) {
		switch (index) {
			case CLNK_ID: {
				*out_funcs = &snp::g_spi_functions;
				snp::g_plugged_network = std::make_unique<clnk::CrownLink>();
				return true;
			} break;
		}
	}
	return false;
}

HINSTANCE g_instance;

static void dll_start() {
	WSADATA wsaData{};
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (auto error_code = WSAStartup(wVersionRequested, &wsaData); error_code != S_OK) {
		g_root_logger.fatal("WSAStartup failed with error {}", error_code);
	}
}

static void dll_exit() {
	WSACleanup();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
	switch (reason) {
		case DLL_PROCESS_ATTACH: {
			g_instance = instance;

			dll_start();
			std::atexit(dll_exit);
		} break;
	}
	return true;
}
