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

void XBMC_log_stub(void *HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg) {
  std::cout << msg << "\n";
};

class Proxy : public ADDON::CHelper_libXBMC_addon {
public:
  Proxy() {
    XBMC_log = (void (*)(void* HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg)) XBMC_log_stub;
  }

};

ADDON::CHelper_libXBMC_addon *xbmc = new Proxy();

