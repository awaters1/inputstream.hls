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
#include "demux/demux.h"
#include "session.h"
#include "decoding.h"
#include "kodi.h"

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
/***************************  Interface *********************************/

#include "kodi_inputstream_dll.h"
#include "libKODI_inputstream.h"

CHelper_libKODI_inputstream *ipsh = 0;

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
        std::vector<hls::Stream> streams = hls_session->get_streams();
        iids.m_streamCount = streams.size();
        for(int i = 0; i < streams.size(); ++i) {
          iids.m_streamIds[i] = streams[i].stream_id;
        }
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
    caps.m_supportsSeek = false;
    // TODO: Configure seek support
    // caps.m_supportsSeek = session && !session->IsLive();
    caps.m_supportsPause = caps.m_supportsSeek;
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
      hls::Stream stream = hls_session->get_stream(streamid);
      if (stream.stream_id == streamid) {
        INPUTSTREAM_INFO stream_info = {};
        if (stream.codec_name == "aac") {
          strcpy(stream_info.m_codecName, "aac");
          stream_info.m_streamType = INPUTSTREAM_INFO::TYPE_AUDIO;
        } else if (stream.codec_name == "h264") {
          strcpy(stream_info.m_codecName, "h264");
          stream_info.m_streamType = INPUTSTREAM_INFO::TYPE_VIDEO;
        }
        stream_info.m_pID = stream.stream_id;
        stream_info.m_Channels = stream.channels;
        stream_info.m_SampleRate = stream.sample_rate;
        stream_info.m_BitRate = stream.bit_rate;
        stream_info.m_BitsPerSample = stream.bits_per_sample;

        return stream_info;
      } else {
        return dummy_info;
      }
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

  int count = 0;

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
    return 10;
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

    TSDemux::STREAM_PKT* pkt = hls_session->get_current_pkt();
    if (!pkt) {
      std::cerr << "Invalid packet" << std::endl;
      return NULL;
    }

    if (pkt->streamChange) {
        DemuxPacket *p = ipsh->AllocateDemuxPacket(0);
        p->iStreamId = DMX_SPECIALID_STREAMCHANGE;
        xbmc->Log(ADDON::LOG_DEBUG, "DMX_SPECIALID_STREAMCHANGE");
        hls_session->read_next_pkt();
        return p;
    } else {
      DemuxPacket *p = ipsh->AllocateDemuxPacket(pkt->size);
      p->dts = pkt->dts * 10;
      p->pts = pkt->pts * 10;
      p->duration = pkt->duration * 10;
      p->iStreamId = pkt->pid;
      p->iGroupId = 0;
      p->iSize = pkt->size;
      // std::cout << "Sending packet for stream " << p->iStreamId << " of size " << p->iSize << "\n";
      memcpy(p->pData, pkt->data, p->iSize);
      hls_session->read_next_pkt();
      return p;
    }
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
    return static_cast<int>(hls_session->get_total_time() * 1000);
  }

  int GetTime()
  {
    if (!hls_session)
      return 0;
    // TODO: Doesnt' get the correct time
    return static_cast<int>(hls_session->get_current_time());
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
