#include <winsock2.h>

#include "SNPModule.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


// these modi are implemented in this DLL
#include "DirectIP.h"
#define DRIP_ID 0
#include "JuiceP2P.h"
#define JP2P_ID 1
//include "LocalPC.h"
//define SMEM_ID 1

BOOL WINAPI SnpQuery(DWORD dwIndex, DWORD *dwNetworkCode, char **ppszNetworkName, char **ppszNetworkDescription, CAPS **ppCaps)
{
  if ( dwNetworkCode && ppszNetworkName && ppszNetworkDescription && ppCaps )
  {
    switch (dwIndex)
    {
    case DRIP_ID:
      *dwNetworkCode          =  DRIP::networkInfo.dwIdentifier;
      *ppszNetworkName        =  DRIP::networkInfo.pszName;
      *ppszNetworkDescription =  DRIP::networkInfo.pszDescription;
      *ppCaps                 = &DRIP::networkInfo.caps;
      return TRUE;
    case JP2P_ID:
      *dwNetworkCode          =  JP2P::networkInfo.dwIdentifier;
      *ppszNetworkName        =  JP2P::networkInfo.pszName;
      *ppszNetworkDescription =  JP2P::networkInfo.pszDescription;
      *ppCaps                 = &JP2P::networkInfo.caps;
      return TRUE;
    default:
      return FALSE;
    }
  }
  return FALSE;
}

BOOL WINAPI SnpBind(DWORD dwIndex, SNP::NetFunctions **ppFxns)
{
  if ( ppFxns )
  {
    switch (dwIndex)
    {
    case DRIP_ID:
      *ppFxns = &SNP::spiFunctions;
      SNP::pluggedNetwork = (SNP::Network<SNP::SNETADDR>*)(new DRIP::DirectIP());
      return TRUE;
    case JP2P_ID:
      *ppFxns = &SNP::spiFunctions;
      SNP::pluggedNetwork = (SNP::Network<SNP::SNETADDR>*)(new JP2P::JuiceP2P());
      return TRUE;
    default:
      return FALSE;
    }
  }
  return FALSE;
}

HINSTANCE hInstance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  switch(fdwReason)
  {
  case DLL_PROCESS_ATTACH:
    hInstance = hinstDLL;
    break;
  case DLL_PROCESS_DETACH:
    break;
  default:
    break;
  }
  return TRUE;
}
