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
#include "xbmc_codec_types.h"

void XBMC_log_stub(void *HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg) {
  std::cout << msg << "\n";
};

class XBMC_Proxy : public ADDON::CHelper_libXBMC_addon {
public:
  XBMC_Proxy() {
    XBMC_log = (void (*)(void* HANDLE, void* CB, const ADDON::addon_log_t loglevel, const char *msg)) XBMC_log_stub;
  }

};

bool g_bExtraDebug = false;
ADDON::CHelper_libXBMC_addon *xbmc = new XBMC_Proxy();

DemuxPacket* IPSH_allocate_demux_packet_stub(void* HANDLE, void* CB, int iDataSize) {
  DemuxPacket *pkt = new DemuxPacket;
  if (iDataSize) {
    pkt->pData = new unsigned char[iDataSize];
  } else {
    pkt->pData = 0;
  }
  return pkt;
}

void IPSH_free_demux_packet_stub(void* HANDLE, void* CB, DemuxPacket* pPacket) {
  if (pPacket->pData) {
    delete [] pPacket->pData;
  }
  delete pPacket;
}

int dlclose (void *__handle) {

}

class IPSH_Proxy : public CHelper_libKODI_inputstream {
public:
  IPSH_Proxy() {
    INPUTSTREAM_allocate_demux_packet = (DemuxPacket* (*)(void* HANDLE, void* CB, int iDataSize)) IPSH_allocate_demux_packet_stub;
    INPUTSTREAM_free_demux_packet = (void (*)(void* HANDLE, void* CB, DemuxPacket* pPacket)) IPSH_free_demux_packet_stub;
  }

};

CHelper_libKODI_inputstream *ipsh = new IPSH_Proxy();

xbmc_codec_t CODEC_get_codec_by_name_stub(void* HANDLE, void* CB, const char* strCodecName) {
  return xbmc_codec_t();
}

class CODEC_Proxy : public CHelper_libXBMC_codec {
public:
  CODEC_Proxy() {
    CODEC_get_codec_by_name = (xbmc_codec_t (*)(void* HANDLE, void* CB, const char* strCodecName)) CODEC_get_codec_by_name_stub;
  }
};

CHelper_libXBMC_codec *CODEC = new CODEC_Proxy();
