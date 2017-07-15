/*
 *      Copyright (C) 2016-2016 peak3d
 *      http://www.peak3d.de
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "MainHLS.h"

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <sstream>

#include "xbmc_addon_types.h"
#include "libXBMC_addon.h"
#include "helpers.h"
#include "kodi_vfs_types.h"
#include "kodi.h"

#include "kodi_inputstream_dll.h"

// HLS specific
#include "hls/session.h"
#include "hls/HLS.h"
#include "kodi_hls.h"
#include "demux_container.h"


/*******************************************************
Main class Session
********************************************************/
KodiSession *hls_session = 0;

KodiHost kodihost;
ADDON::CHelper_libXBMC_addon *xbmc = nullptr;
std::uint16_t kodiDisplayWidth(0), kodiDisplayHeight(0);

bool g_bExtraDebug = true;
/***************************  Interface *********************************/

#include "libKODI_inputstream.h"

CHelper_libKODI_inputstream *ipsh = 0;
CHelper_libXBMC_codec *CODEC = 0;

extern "C" {

  ADDON_STATUS curAddonStatus = ADDON_STATUS_UNKNOWN;

  /***********************************************************
  * Standard AddOn related public library functions
  ***********************************************************/

  ADDON_STATUS ADDON_Create(void* hdl, void* props)
  {
    // initialize globals
    hls_session = nullptr;
    kodiDisplayWidth = 1280;
    kodiDisplayHeight = 720;

    if (!hdl)
      return ADDON_STATUS_UNKNOWN;

    xbmc = new ADDON::CHelper_libXBMC_addon;
    if (!xbmc->RegisterMe(hdl))
    {
      SAFE_DELETE(xbmc);
      return ADDON_STATUS_PERMANENT_FAILURE;
    }
    xbmc->Log(ADDON::LOG_DEBUG, "libXBMC_addon successfully loaded");

    ipsh = new CHelper_libKODI_inputstream;
    if (!ipsh->RegisterMe(hdl))
    {
      SAFE_DELETE(xbmc);
      SAFE_DELETE(ipsh);
      return ADDON_STATUS_PERMANENT_FAILURE;
    }

    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_Create()");

    CODEC = new CHelper_libXBMC_codec;
    if (!CODEC->RegisterMe(hdl))
    {
      SAFE_DELETE(xbmc);
      SAFE_DELETE(ipsh);
      SAFE_DELETE(CODEC);
      return ADDON_STATUS_PERMANENT_FAILURE;
    }

    curAddonStatus = ADDON_STATUS_OK;
    return curAddonStatus;
  }

  ADDON_STATUS ADDON_GetStatus()
  {
    return curAddonStatus;
  }

  void ADDON_Destroy()
  {
    SAFE_DELETE(hls_session);
    if (xbmc)
    {
      xbmc->Log(ADDON::LOG_DEBUG, "ADDON_Destroy()");
      SAFE_DELETE(xbmc);
    }
    SAFE_DELETE(ipsh);
  }

  bool ADDON_HasSettings()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_HasSettings()");
    return false;
  }

  unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_GetSettings()");
    return 0;
  }

  ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_SetSettings()");
    return ADDON_STATUS_OK;
  }

  void ADDON_Stop()
  {
  }

  void ADDON_FreeSettings()
  {
  }

  void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
  {
  }

  /***********************************************************
  * InputSteam Client AddOn specific public library functions
  ***********************************************************/

  bool Open(INPUTSTREAM& props)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "Open()");

    const char *lt(""), *lk(""), *ld("");
    for (unsigned int i(0); i < props.m_nCountInfoValues; ++i)
    {
      if (strcmp(props.m_ListItemProperties[i].m_strKey, "inputstream.mpd.license_type") == 0)
      {
        xbmc->Log(ADDON::LOG_DEBUG, "found inputstream.mpd.license_type: %s", props.m_ListItemProperties[i].m_strValue);
        lt = props.m_ListItemProperties[i].m_strValue;
      }
      else if (strcmp(props.m_ListItemProperties[i].m_strKey, "inputstream.mpd.license_key") == 0)
      {
        xbmc->Log(ADDON::LOG_DEBUG, "found inputstream.mpd.license_key: [not shown]");
        lk = props.m_ListItemProperties[i].m_strValue;
      }
      else if (strcmp(props.m_ListItemProperties[i].m_strKey, "inputstream.mpd.license_data") == 0)
      {
        xbmc->Log(ADDON::LOG_DEBUG, "found inputstream.mpd.license_data: [not shown]");
        ld = props.m_ListItemProperties[i].m_strValue;
      }
    }

    kodihost.SetProfilePath(props.m_profileFolder);

    std::string fn(std::string(props.m_profileFolder) + "bandwidth.bin");
    FILE* f = fopen(fn.c_str(), "rb");
    double bandwidth = 4000000;
    if (f) {
      fread(&bandwidth, sizeof(double), 1, f);
      fclose(f);
    }
    xbmc->Log(ADDON::LOG_DEBUG, "Initial bandwidth: %f ", bandwidth);
    std::unordered_map<StateAction, double> q_map;
    fn = std::string(props.m_profileFolder) + "q_map.bin";
    f = fopen(fn.c_str(), "rb");
    if (f) {
      while(true) {
        uint32_t buff_s;
        uint32_t bw_kbps;
        uint32_t prev_qual;
        uint32_t curr_qual;
        double value;
        size_t ret = fread(&buff_s, sizeof(uint32_t), 1, f);
        if (!ret) {
          break;
        }
        fread(&bw_kbps, sizeof(uint32_t), 1, f);
        fread(&prev_qual, sizeof(uint32_t), 1, f);
        fread(&curr_qual, sizeof(uint32_t), 1, f);
        fread(&value, sizeof(double), 1, f);
        StateAction state_action(
            State(buff_s * 1000, bw_kbps, prev_qual * 1024),
            Action(curr_qual * 1024)
        );
        q_map[state_action] = value;
      }
      fclose(f);
    }
    std::unordered_map<State, double> explore_map;
    fn = std::string(props.m_profileFolder) + "explore_map.bin";
    f = fopen(fn.c_str(), "rb");
    if (f) {
      while(true) {
        uint32_t buff_s;
        uint32_t bw_kbps;
        uint32_t prev_qual;
        double value;
        size_t ret = fread(&buff_s, sizeof(uint32_t), 1, f);
        if (!ret) {
          break;
        }
        fread(&bw_kbps, sizeof(uint32_t), 1, f);
        fread(&prev_qual, sizeof(uint32_t), 1, f);
        fread(&value, sizeof(double), 1, f);
        State state(buff_s * 1000, bw_kbps, prev_qual * 1024);
        explore_map[state] = value;
      }
      fclose(f);
    }

    int min_bandwidth(0);
    xbmc->GetSetting("MINBANDWIDTH", (char*)&min_bandwidth);
    min_bandwidth = min_bandwidth * 1000;
    xbmc->Log(ADDON::LOG_DEBUG, "MINBANDWIDTH selected: %d ", min_bandwidth);
    int max_bandwidth(0);
    xbmc->GetSetting("MAXBANDWIDTH", (char*)&max_bandwidth);
    max_bandwidth = max_bandwidth * 1000;
    xbmc->Log(ADDON::LOG_DEBUG, "MAXBANDWIDTH selected: %d ", max_bandwidth);
    int buf(0);
    xbmc->GetSetting("STREAMSELECTION", (char*)&buf);
    xbmc->Log(ADDON::LOG_DEBUG, "STREAMSELECTION selected: %d ", buf);
    bool manual_streams = buf != 0;

    KodiMasterPlaylist master_playlist;
    master_playlist.open(props.m_strURL);
    master_playlist.select_media_playlist();
    hls_session = new KodiSession(master_playlist, bandwidth, props.m_profileFolder,
        min_bandwidth, max_bandwidth, manual_streams, q_map, explore_map);

    return true;
  }

  void Close(void)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "Close()");
    SAFE_DELETE(hls_session);
  }

  const char* GetPathList(void)
  {
    return "";
  }

  struct INPUTSTREAM_IDS GetStreamIds()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "GetStreamIds()");
    INPUTSTREAM_IDS iids;

    if(hls_session)
    {
        return hls_session->get_streams();
    } else
        iids.m_streamCount = 0;

    return iids;
  }

  struct INPUTSTREAM_CAPABILITIES GetCapabilities()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "GetCapabilities()");
    INPUTSTREAM_CAPABILITIES caps;
    caps.m_supportsIDemux = true;
    caps.m_supportsIPosTime = false;
    caps.m_supportsIDisplayTime = true;
    caps.m_supportsSeek = true;
    caps.m_supportsPause = true; //caps.m_supportsSeek;
    return caps;
  }

  struct INPUTSTREAM_INFO GetStream(int streamid)
  {
    static struct INPUTSTREAM_INFO dummy_info = {
      INPUTSTREAM_INFO::TYPE_NONE, "", "", 0, 0, 0, 0, "",
      0, 0, 0, 0, 0.0f,
      0, 0, 0, 0, 0 };

    xbmc->Log(ADDON::LOG_DEBUG, "GetStream(%d)", streamid);

    if(hls_session) {
      INPUTSTREAM_INFO stream_info = hls_session->get_stream(streamid);
      memcpy(&dummy_info, &stream_info, sizeof(INPUTSTREAM_INFO));
      return dummy_info;
    }
    return dummy_info;
  }

  void EnableStream(int streamid, bool enable)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "EnableStream(%d: %s)", streamid, enable?"true":"false");
    // TODO: Unsure if I need to support EnableStream
  }

  // Doesn't cause any skpping, so it is something related
  // to how the streams are demuxed
  int ReadStream(unsigned char* buf, unsigned int size)
  {
    return -1;
  }

  int64_t SeekStream(int64_t, int)
  {
    return -1;
  }

  int64_t PositionStream(void)
  {
    return -1;
  }

  int64_t LengthStream(void)
  {
    return -1;
  }

  void DemuxReset(void)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "DemuxReset");
  }

  void DemuxAbort(void)
  {
    // Called when stopping, not sure if it is wise to stop
    // all threads, perhaps just stop processing data and cancel
    // any downloads
    xbmc->Log(ADDON::LOG_DEBUG, "DemuxAbort");
    if (hls_session) {
      hls_session->demux_abort();
    }
  }

  void DemuxFlush(void)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "DemuxFlush");
    if (hls_session) {
      hls_session->demux_flush();
    }
  }

  DemuxPacket* __cdecl DemuxRead(void)
  {
    if (!hls_session)
      return NULL;

    DemuxContainer demux_container = hls_session->get_current_pkt();
    DemuxPacket *packet = demux_container.demux_packet;
    /*
    if (!packet) {
      return ipsh->AllocateDemuxPacket(0);
    }
    */
    hls_session->read_next_pkt();
    return packet;
  }

  bool DemuxSeekTime(double time, bool backwards, double *startpts)
  {
    return hls_session->seek_time(time, backwards, startpts);
  }

  void DemuxSetSpeed(int speed)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "DemuxSetSpeed");
  }

  //callback - will be called from kodi
  void SetVideoResolution(int width, int height)
  {
    xbmc->Log(ADDON::LOG_INFO, "SetVideoResolution (%d x %d)", width, height);
  }

  int GetTotalTime()
  {
    if (!hls_session)
      return -1;
    return static_cast<int>(hls_session->get_total_time());
  }

  int GetTime()
  {
    if (!hls_session)
      return -1;
    return hls_session->get_current_time();
  }

  bool CanPauseStream(void)
  {
    return true;
  }

  bool CanSeekStream(void)
  {
    return true;
  }

  bool PosTime(int)
  {
    return false;
  }

  void SetSpeed(int)
  {
  }

  void PauseStream(double)
  {
  }

  bool IsRealTimeStream(void)
  {
    return false;
  }

}//extern "C"
