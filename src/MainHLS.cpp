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
#include <stdio.h>
#include <string.h>
#include <sstream>

#include "xbmc_addon_types.h"
#include "libXBMC_addon.h"
#include "helpers.h"
#include "kodi_vfs_types.h"
#include "SSD_dll.h"
#include "kodi.h"

#include "kodi_inputstream_dll.h"

// HLS specific
#include "hls/session.h"
#include "hls/HLS.h"
#include "kodi_hls.h"


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


    KodiMasterPlaylist master_playlist;
    master_playlist.open(props.m_strURL);
    master_playlist.select_media_playlist();
    hls_session = new KodiSession(master_playlist);

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
    caps.m_supportsSeek = false;//hls_session && !hls_session->is_live();
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
//
//    if (!session)
//      return;
//
//    Session::STREAM *stream(session->GetStream(streamid));
//
//    if (!stream)
//      return;
//
//    if (enable)
//    {
//      if (stream->enabled)
//        return;
//
//      stream->enabled = true;
//
//      stream->stream_.start_stream(~0, session->GetWidth(), session->GetHeight());
//      const dash::DASHTree::Representation *rep(stream->stream_.getRepresentation());
//      xbmc->Log(ADDON::LOG_DEBUG, "Selecting stream with conditions: w: %u, h: %u, bw: %u",
//        stream->stream_.getWidth(), stream->stream_.getHeight(), stream->stream_.getBandwidth());
//
//      if (!stream->stream_.select_stream(true, false, stream->info_.m_pID >> 16))
//      {
//        xbmc->Log(ADDON::LOG_ERROR, "Unable to select stream!");
//        return stream->disable();
//      }
//
//      if(rep != stream->stream_.getRepresentation())
//      {
//        session->UpdateStream(*stream);
//        session->CheckChange(true);
//      }
//
//      stream->input_ = new AP4_DASHStream(&stream->stream_);
//      stream->input_file_ = new AP4_File(*stream->input_, AP4_DefaultAtomFactory::Instance, true);
//      AP4_Movie* movie = stream->input_file_->GetMovie();
//      if (movie == NULL)
//      {
//        xbmc->Log(ADDON::LOG_ERROR, "No MOOV in stream!");
//        return stream->disable();
//      }
//
//      static const AP4_Track::Type TIDC[dash::DASHTree::STREAM_TYPE_COUNT] =
//      { AP4_Track::TYPE_UNKNOWN, AP4_Track::TYPE_VIDEO, AP4_Track::TYPE_AUDIO, AP4_Track::TYPE_TEXT };
//
//      AP4_Track *track = movie->GetTrack(TIDC[stream->stream_.get_type()]);
//      if (!track)
//      {
//        xbmc->Log(ADDON::LOG_ERROR, "No suitable track found in stream");
//        return stream->disable();
//      }
//
//      stream->reader_ = new FragmentedSampleReader(stream->input_, movie, track, streamid, session->GetSingleSampleDecryptor(), session->GetPresentationTimeOffset());
//      stream->reader_->SetObserver(dynamic_cast<FragmentObserver*>(session));
//
//      return;
//    }
//    return stream->disable();
  }

  // Doesn't cause any skpping, so it is something related
  // to how the streams are demuxed
  int ReadStream(unsigned char* buf, unsigned int size)
  {
    if (hls_session) {
      hls_session->read_stream(buf, size);
      return size;
    }
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
  }

  void DemuxAbort(void)
  {
  }

  void DemuxFlush(void)
  {
  }

  DemuxPacket* __cdecl DemuxRead(void)
  {
    if (!hls_session)
      return NULL;

    DemuxPacket *packet = hls_session->get_current_pkt();
    std::cout << "Packet PID: " << packet->iStreamId << " PTS: " << packet->pts << " DTS: " << packet->dts << "\n";
    hls_session->read_next_pkt();
    return packet;
  }

  bool DemuxSeekTime(double time, bool backwards, double *startpts)
  {
    if (!hls_session)
      return false;

    xbmc->Log(ADDON::LOG_INFO, "DemuxSeekTime (%0.4lf)", time);

    // TODO: Support seek time
    // return session->SeekTime(time * 0.001f, 0, !backwards);
    return false;
  }

  void DemuxSetSpeed(int speed)
  {

  }

  //callback - will be called from kodi
  void SetVideoResolution(int width, int height)
  {
    xbmc->Log(ADDON::LOG_INFO, "SetVideoResolution (%d x %d)", width, height);
    if (hls_session) {
      // TODO: Support set video resolution
      // session->SetVideoResolution(width, height);
    } else
    {
      kodiDisplayWidth = width;
      kodiDisplayHeight = height;
    }
  }

  int GetTotalTime()
  {
    if (!hls_session)
      return 0;
    // TODO: Doesn't work for live streams
    return static_cast<int>(hls_session->get_total_time() * 1000);
  }

  int GetTime()
  {
    if (!hls_session)
      return 0;
    // TODO: Doesnt' get the correct time
    // return static_cast<int>((double)(hls_session->get_current_time())/ 90.0);
    return static_cast<int>((double)(hls_session->get_current_time())/ 90.0);
  }

  bool CanPauseStream(void)
  {
    return true;
  }

  bool CanSeekStream(void)
  {
    // TODO: Support can seek stream
    return false;
    // return session && !session->IsLive();
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
