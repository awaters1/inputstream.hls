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

#include <iostream>
#include <string.h>
#include <sstream>

#include "xbmc_addon_types.h"
#include "libXBMC_addon.h"
#include "kodi_inputstream_types.h"

#include "Ap4.h"

#define SAFE_DELETE(p)       do { delete (p);     (p)=NULL; } while (0)

ADDON::CHelper_libXBMC_addon *xbmc = 0;

/*******************************************************
|   FragmentedSampleReader
********************************************************/
class FragmentedSampleReader : public AP4_LinearReader
{
public:

  FragmentedSampleReader(AP4_ByteStream *input, AP4_Movie *movie, AP4_Track *track, AP4_UI32 streamId)
    : AP4_LinearReader(*movie, input)
    , m_Track(track)
    , m_ts(0.0)
    , m_eos(false)
    , m_StreamId(streamId)
  {
    EnableTrack(m_Track->GetId());
  }

  ~FragmentedSampleReader()
  {
  }

  AP4_Result ReadSample()
  {
    AP4_Result result;
    if (AP4_FAILED(result = ReadNextSample(m_Track->GetId(), m_sample_, m_sample_data_)))
    {
      if (result == AP4_ERROR_EOS) {
        m_eos = true;
      }
      else {
        return result;
      }
    }
    m_ts = (double)m_sample_.GetDts() / (double)m_Track->GetMediaTimeScale();
    return AP4_SUCCESS;
  };

  bool EOS()const{ return m_eos; };
  double DTS()const{ return m_ts; };
  const AP4_Sample &Sample()const { return m_sample_; };
  AP4_UI32 GetStreamId()const{ return m_StreamId; };
  AP4_Size GetSampleDataSize()const{ return m_sample_data_.GetDataSize(); };
  const AP4_Byte *GetSampleData()const{ return m_sample_data_.GetData(); };

private:
  AP4_Track *m_Track;
  AP4_UI32 m_StreamId;
  bool m_eos;
  double m_ts;

  AP4_Sample     m_sample_;
  AP4_DataBuffer m_sample_data_;
};

/*******************************************************
Main class Session
********************************************************/
class Session
{
public:
  Session();
  ~Session();
  bool initialize();
  void SetStreamProperties(uint16_t width, uint16_t height, const char* language, uint32_t maxBitPS, bool allow_ec_3);
  FragmentedSampleReader *GetNextSample();
  INPUTSTREAM_INFO *GetStreamInfo(unsigned int sid){ return sid == 1 ? &audio_info_ : sid == 2 ? &video_info_ : 0; };
private:
  AP4_ByteStream *video_input_, *audio_input_;
  AP4_File *video_input_file_, *audio_input_file_;
  INPUTSTREAM_INFO video_info_, audio_info_;

  uint16_t width_, height_;
  std::string language_;
  uint32_t fixed_bandwidth_;

  FragmentedSampleReader *audio_reader_, *video_reader_;
} *session = 0;

Session::Session()
  : video_input_(NULL)
  , audio_input_(NULL)
  , video_input_file_(NULL)
  , audio_input_file_(NULL)
  , audio_reader_(NULL)
  , video_reader_(NULL)
{
  memset(&audio_info_, 0, sizeof(audio_info_));
  memset(&video_info_, 0, sizeof(video_info_));

  audio_info_.m_streamType = INPUTSTREAM_INFO::TYPE_AUDIO;
  audio_info_.m_pID = 1;

  video_info_.m_streamType = INPUTSTREAM_INFO::TYPE_VIDEO;
  video_info_.m_pID = 2;
}

Session::~Session()
{
  delete video_reader_;
  delete video_input_;

  delete audio_reader_;
  delete audio_input_;
}

/*----------------------------------------------------------------------
|   initialize
+---------------------------------------------------------------------*/

static bool copyLang(char* dest, const char* src)
{
  size_t len(strlen(src));

  if (len && len != 3)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Invalid language in trak atom (%s)", src);
    return false;
  }
  strcpy(dest, src);
  return true;
}

bool Session::initialize()
{
  AP4_Result result;
  /************ VIDEO INITIALIZATION ******/
  result = AP4_FileByteStream::Create("C:\\Temp\\video.mov", AP4_FileByteStream::STREAM_MODE_READ, video_input_);
  if (AP4_FAILED(result)) {
    xbmc->Log(ADDON::LOG_ERROR, "Cannot open video.mov!");
    return false;
  }
  video_input_file_ = new AP4_File(*video_input_, AP4_DefaultAtomFactory::Instance, true);
  AP4_Movie* movie = video_input_file_->GetMovie();
  if (movie == NULL)
  {
    xbmc->Log(ADDON::LOG_ERROR, "No MOOV in video stream!");
    return false;
  }
  AP4_Track *track = movie->GetTrack(AP4_Track::TYPE_VIDEO);
  if (!track)
  {
    xbmc->Log(ADDON::LOG_ERROR, "No suitable track found in video stream");
    return false;
  }

  AP4_SampleDescription *desc = track->GetSampleDescription(0);
  if (desc->GetType() == AP4_SampleDescription::TYPE_PROTECTED)
    desc = static_cast<AP4_ProtectedSampleDescription*>(desc)->GetOriginalSampleDescription();
  AP4_VideoSampleDescription *video_sample_description = AP4_DYNAMIC_CAST(AP4_VideoSampleDescription, desc);
  if (video_sample_description == NULL)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Unable to parse video sample description!");
    return false;
  }
  switch (desc->GetFormat())
  {
  case AP4_SAMPLE_FORMAT_AVC1:
  case AP4_SAMPLE_FORMAT_AVC2:
  case AP4_SAMPLE_FORMAT_AVC3:
  case AP4_SAMPLE_FORMAT_AVC4:
    strcpy(video_info_.m_codecName, "H264");
    break;
  case AP4_SAMPLE_FORMAT_HEV1:
  case AP4_SAMPLE_FORMAT_HVC1:
    strcpy(video_info_.m_codecName, "HEVC");
    break;
  default:
    xbmc->Log(ADDON::LOG_ERROR, "Video codec not supported");
    return false;
  }
  video_info_.m_Width = video_sample_description->GetWidth();
  video_info_.m_Height = video_sample_description->GetHeight();
  video_info_.m_Aspect = 1.0;

  video_reader_ = new FragmentedSampleReader(video_input_, movie, track, 1);

  if (!AP4_SUCCEEDED(video_reader_->ReadSample()))
    return false;

  /************ AUDIO INITIALIZATION ******/
  result = AP4_FileByteStream::Create("C:\\Temp\\audio.mov", AP4_FileByteStream::STREAM_MODE_READ, audio_input_);
  if (AP4_FAILED(result)) {
    xbmc->Log(ADDON::LOG_ERROR, "Cannot open audio.mov!");
    return false;
  }
  audio_input_file_ = new AP4_File(*audio_input_, AP4_DefaultAtomFactory::Instance, true);
  movie = audio_input_file_->GetMovie();
  if (movie == NULL)
  {
    xbmc->Log(ADDON::LOG_ERROR, "No MOOV in audio stream!");
    return false;
  }
  track = movie->GetTrack(AP4_Track::TYPE_AUDIO);
  if (!track)
  {
    xbmc->Log(ADDON::LOG_ERROR, "No suitable track found in audio stream!");
    return false;
  }

  if (!copyLang(audio_info_.m_language, track->GetTrackLanguage()))
    return false;

  desc = track->GetSampleDescription(0);
  if (desc->GetType() == AP4_SampleDescription::TYPE_PROTECTED)
    desc = static_cast<AP4_ProtectedSampleDescription*>(desc)->GetOriginalSampleDescription();
  AP4_AudioSampleDescription *audio_sample_description = AP4_DYNAMIC_CAST(AP4_AudioSampleDescription, desc);
  if (audio_sample_description == NULL)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Unable to parse audio sample description!");
    return false;
  }
  switch (desc->GetFormat())
  {
  case AP4_SAMPLE_FORMAT_MP4A:
    strcpy(audio_info_.m_codecName, "AAC");
    break;
  case  AP4_SAMPLE_FORMAT_AC_3:
  case AP4_SAMPLE_FORMAT_EC_3:
    strcpy(audio_info_.m_codecName, "EAC3");
    break;
  default:
    xbmc->Log(ADDON::LOG_ERROR, "Audio codec not supported!");
    return false;
  }
  if (AP4_MpegSystemSampleDescription *esds = AP4_DYNAMIC_CAST(AP4_MpegSystemSampleDescription, audio_sample_description))
    audio_info_.m_BitRate = esds->GetAvgBitrate();

  audio_info_.m_BitsPerSample = audio_sample_description->GetSampleSize();
  audio_info_.m_Channels = audio_sample_description->GetChannelCount();
  audio_info_.m_SampleRate = audio_sample_description->GetSampleRate();

  audio_reader_ = new FragmentedSampleReader(audio_input_, movie, track, 0);

  if (!AP4_SUCCEEDED(audio_reader_->ReadSample()))
    return false;

  return true;
}

FragmentedSampleReader *Session::GetNextSample()
{
  FragmentedSampleReader *stack[2];
  unsigned int numReader(0);

  if (!video_reader_->EOS())
    stack[numReader++] = video_reader_;
  if (!audio_reader_->EOS())
    stack[numReader++] = audio_reader_;

  FragmentedSampleReader *res(0);

  while (numReader--)
    if (!res || stack[numReader]->DTS() < res->DTS())
      res = stack[numReader];

  return res;
}

/***************************  Interface *********************************/

#include "kodi_inputstream_dll.h"
#include "libKODI_inputstream.h"

extern "C" {
  
  ADDON_STATUS curAddonStatus = ADDON_STATUS_UNKNOWN;
  CHelper_libKODI_inputstream *ipsh = 0;

  /***********************************************************
  * Standart AddOn related public library functions
  ***********************************************************/

  ADDON_STATUS ADDON_Create(void* hdl, void* props)
  {
    if (!hdl)
      return ADDON_STATUS_UNKNOWN;

    xbmc = new ADDON::CHelper_libXBMC_addon;
    if (!xbmc->RegisterMe(hdl))
    {
      SAFE_DELETE(xbmc);
      return ADDON_STATUS_PERMANENT_FAILURE;
    }

    ipsh = new CHelper_libKODI_inputstream;
    if (!ipsh->RegisterMe(hdl))
    {
      SAFE_DELETE(xbmc);
      SAFE_DELETE(ipsh);
      return ADDON_STATUS_PERMANENT_FAILURE;
    }

    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: ADDON_Create()");

    curAddonStatus = ADDON_STATUS_UNKNOWN;

    //if (XBMC->GetSetting("host", buffer))

    curAddonStatus = ADDON_STATUS_OK;
    return curAddonStatus;
  }

  ADDON_STATUS ADDON_GetStatus()
  {
    return curAddonStatus;
  }

  void ADDON_Destroy()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: ADDON_Destroy()");
    SAFE_DELETE(session);
    SAFE_DELETE(xbmc);
    SAFE_DELETE(ipsh);
  }

  bool ADDON_HasSettings()
  {
    return false;
  }

  unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
  {
    return 0;
  }

  ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
  {
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
    char buf[1024];
    GetCurrentDirectory(1024, buf);
    
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: OpenStream(%s)", buf);

    session = new Session();
    if (!session->initialize())
    {
      SAFE_DELETE(session);
      return false;
    }
    return true;
  }

  void Close(void)
  {
    SAFE_DELETE(session);
  }

  const char* GetPathList(void)
  {
    static char buffer[1024];

    if (!xbmc->GetSetting("URL", buffer))
      buffer[0] = 0;

    return buffer;
  }

  struct INPUTSTREAM_IDS GetStreamIds()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: GetStreamIds()");
    INPUTSTREAM_IDS iids;
    if (session)
    {
      iids.m_streamCount = 2;
      iids.m_streamIds[0] = 1;
      iids.m_streamIds[1] = 2;
    } else
      iids.m_streamCount = 0;

    return iids;
  }

  struct INPUTSTREAM_CAPABILITIES GetCapabilities()
  {
    INPUTSTREAM_CAPABILITIES caps;
    caps.m_supportsIDemux = true;
    caps.m_supportsISeekTime = false;
    caps.m_supportsIDisplayTime = true;
    return caps;
  }

  struct INPUTSTREAM_INFO GetStream(int streamid)
  {
    static struct INPUTSTREAM_INFO dummy_info = {
      INPUTSTREAM_INFO::TYPE_NONE, "", 0, "",
      0, 0, 0, 0, 0.0f,
      0, 0, 0, 0, 0 };
    
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: GetStream(%d)", streamid);

    if (session)
    {
      INPUTSTREAM_INFO *info(session->GetStreamInfo(streamid));
      if (info)
        return *info;
    }
    return dummy_info;
  }

  void EnableStream(int streamid, bool enable)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: EnableStream(%d, %d)", streamid, (int)enable);
  }

  int ReadStream(unsigned char*, unsigned int)
  {
    return -1;
  }

  long long SeekStream(long long, int)
  {
    return -1;
  }

  long long PositionStream(void)
  {
    return -1;
  }

  long long LengthStream(void)
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
    if (!session)
      return NULL;

    FragmentedSampleReader *sr(session->GetNextSample());

    if (sr)
    {
      const AP4_Sample &s(sr->Sample());
      DemuxPacket *p = ipsh->AllocateDemuxPacket(sr->GetSampleDataSize());
      p->dts = sr->DTS();
      p->pts = p->dts;
      p->duration = s.GetDuration();
      p->iStreamId = sr->GetStreamId();
      p->iGroupId = 0;
      p->iSize = sr->GetSampleDataSize();
      memcpy(p->pData, sr->GetSampleData(), p->iSize);

      sr->ReadSample();
      return p;
    }
    return NULL;
  }

  bool DemuxSeekTime(int time, bool backwards, double *startpts)
  {
    return false;
  }

  void DemuxSetSpeed(int speed)
  {

  }

  int GetTotalTime()
  {
    return 20;
  }

  int GetTime()
  {
    return 0;
  }

  bool CanPauseStream(void)
  {
    return true;
  }

  void PauseStream(bool)
  {
  }

  bool CanSeekStream(void)
  {
    return false;
  }

  bool SeekTime(int)
  {
    return false;
  }

  void SetSpeed(int)
  {
  }

  bool IsRealTimeStream(void)
  {
    return false;
  }

}//extern "C"
