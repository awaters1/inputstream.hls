/*
 * xbmc.cpp
 *
 *  Created on: Apr 6, 2017
 *      Author: watersan
 */

/*
class Xbmc {
  void Log(const int log_level, const char *format, ... );
};

Xbmc *xbmc = new Xbmc();
*/

#include <iostream>
#include "libXBMC_addon.h"
#include "libKODI_inputstream.h"
#include "libXBMC_codec.h"

void XBMC_log_stub(void *HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg) {
  std::cout << msg << "\n";
};

class XBMC_Proxy : public ADDON::CHelper_libXBMC_addon {
public:
  XBMC_Proxy() {
    XBMC_log = (void (*)(void* HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg)) XBMC_log_stub;
  }

};

bool g_bExtraDebug = true;
ADDON::CHelper_libXBMC_addon *xbmc = new XBMC_Proxy();

class IPSH_Proxy : public CHelper_libKODI_inputstream {
public:
  IPSH_Proxy() {

  }

};

CHelper_libKODI_inputstream *ipsh = new IPSH_Proxy();

class CODEC_Proxy : public CHelper_libXBMC_codec {

};

CHelper_libXBMC_codec *CODEC = new CODEC_Proxy();
