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

#include "MainDash.h"

#include <iostream>
#include <string.h>
#include <sstream>

#include "xbmc_addon_types.h"
#include "libXBMC_addon.h"
#include "helpers.h"

#define SAFE_DELETE(p)       do { delete (p);     (p)=NULL; } while (0)

ADDON::CHelper_libXBMC_addon *xbmc = 0;

/*******************************************************
Bento4 Streams
********************************************************/

class AP4_DASHStream : public AP4_ByteStream
{
public:
  // Constructor
  AP4_DASHStream(dash::DASHStream *dashStream) :dash_stream_(dashStream){};

  // AP4_ByteStream methods
  AP4_Result ReadPartial(void*    buffer,
    AP4_Size  bytesToRead,
    AP4_Size& bytesRead) override
  {
    bytesRead = dash_stream_->read(buffer, bytesToRead);
    return bytesRead > 0 ? AP4_SUCCESS : AP4_ERROR_READ_FAILED;
  };
  AP4_Result WritePartial(const void* buffer,
    AP4_Size    bytesToWrite,
    AP4_Size&   bytesWritten) override
  {
    /* unimplemented */
    return AP4_ERROR_NOT_SUPPORTED;
  };
  AP4_Result Seek(AP4_Position position) override
  {
    return dash_stream_->seek(position) ? AP4_SUCCESS : AP4_ERROR_NOT_SUPPORTED;
  };
  AP4_Result Tell(AP4_Position& position) override
  {
    position = dash_stream_->tell();
    return AP4_SUCCESS;
  };
  AP4_Result GetSize(AP4_LargeSize& size) override
  {
    /* unimplemented */
    return AP4_ERROR_NOT_SUPPORTED;
  };
  // AP4_Referenceable methods
  void AddReference() override {};
  void Release()override      {};
protected:
  // members
  dash::DASHStream *dash_stream_;
};

/*******************************************************
Kodi Streams implementation
********************************************************/

bool KodiDASHTree::download(const char* url)
{
  // open the file
  std::string strURL(url);
  strURL += "|encoding%3Dgzip";
    
  void* file = xbmc->OpenFile(strURL.c_str(), XBMCFILE::READ_CHUNKED | XBMCFILE::READ_NO_CACHE);
  if (!file)
    return false;
  
  // read the file
  char buf[8192];
  size_t nbRead;
  while ((nbRead = xbmc->ReadFile(file, buf, 8192)) > 0 && write_data(buf, nbRead));

  xbmc->CloseFile(file);

  return nbRead == 0;
}

bool KodiDASHStream::download(const char* url)
{
  // open the file
  void* file = xbmc->OpenFile(url, XBMCFILE::READ_CHUNKED | XBMCFILE::READ_NO_CACHE);
  if (!file)
    return false;

  // read the file
  char *buf = (char*)malloc(1024*1024);
  size_t nbRead;
  while ((nbRead = xbmc->ReadFile(file, buf, 1024 * 1024)) > 0 && write_data(buf, nbRead));
  free(buf);

  xbmc->CloseFile(file);

  return nbRead == 0;
}

/*******************************************************
|   CodecHandler
********************************************************/

class CodecHandler
{
public:
  CodecHandler(AP4_SampleDescription *sd)
    : sample_description(sd)
    , extra_data(0)
    , extra_data_size(0)
    , naluLengthSize(0)
    , pictureId(0)
    , pictureIdPrev(0)
  {};
  virtual AP4_UI08 UpdatePPSId(AP4_DataBuffer &){ return 0; };
  virtual bool GetVideoInformation(unsigned int &width, unsigned int &height){ return false; };

  AP4_SampleDescription *sample_description;
  const AP4_UI08 *extra_data;
  AP4_Size extra_data_size;
  AP4_UI08 naluLengthSize;
  AP4_UI08 pictureId, pictureIdPrev;

};

/***********************   AVC   ************************/

class AVCCodecHandler : public CodecHandler
{
public:
  AVCCodecHandler(AP4_SampleDescription *sd)
    :CodecHandler(sd)
  {
    unsigned int width(0), height(0);
    if (AP4_VideoSampleDescription *video_sample_description = AP4_DYNAMIC_CAST(AP4_VideoSampleDescription, sample_description))
    {
      width = video_sample_description->GetWidth();
      height = video_sample_description->GetHeight();
    }
    if (AP4_AvcSampleDescription *avc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sample_description))
    {
      extra_data_size = avc->GetRawBytes().GetDataSize();
      extra_data = avc->GetRawBytes().GetData();
      if (avc->GetPictureParameters().ItemCount() > 1 || !width || !height)
        naluLengthSize = avc->GetNaluLengthSize();
    }
  }

  virtual AP4_UI08 UpdatePPSId(AP4_DataBuffer const &buffer)
  {
    //Search the Slice header NALU
    const AP4_UI08 *data(buffer.GetData());
    unsigned int data_size(buffer.GetDataSize());
    for (; data_size;)
    {
      // sanity check
      if (data_size < naluLengthSize)
        break;

      // get the next NAL unit
      AP4_UI32 nalu_size;
      switch (naluLengthSize) {
      case 1:nalu_size = *data++; data_size--; break;
      case 2:nalu_size = AP4_BytesToInt16BE(data); data += 2; data_size -= 2; break;
      case 4:nalu_size = AP4_BytesToInt32BE(data); data += 4; data_size -= 4; break;
      default: data_size = 0; nalu_size = 1; break;
      }
      if (nalu_size > data_size)
        break;

      unsigned int nal_unit_type = *data & 0x1F;

      if (nal_unit_type == AP4_AVC_NAL_UNIT_TYPE_CODED_SLICE_OF_NON_IDR_PICTURE ||
        nal_unit_type == AP4_AVC_NAL_UNIT_TYPE_CODED_SLICE_OF_IDR_PICTURE ||
        nal_unit_type == AP4_AVC_NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A ||
        nal_unit_type == AP4_AVC_NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B ||
        nal_unit_type == AP4_AVC_NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C) {

        AP4_DataBuffer unescaped(data, data_size);
        AP4_NalParser::Unescape(unescaped);
        AP4_BitReader bits(unescaped.GetData(), unescaped.GetDataSize());

        bits.SkipBits(8); // NAL Unit Type

        AP4_AvcFrameParser::ReadGolomb(bits); // first_mb_in_slice
        AP4_AvcFrameParser::ReadGolomb(bits); // slice_type
        return AP4_AvcFrameParser::ReadGolomb(bits); //picture_set_id
      }
      // move to the next NAL unit
      data += nalu_size;
      data_size -= nalu_size;
    }
    return 0;
  }

  virtual bool GetVideoInformation(unsigned int &width, unsigned int &height)
  {
    if (pictureId == pictureIdPrev)
      return false;
    pictureIdPrev = pictureId;

    if (AP4_AvcSampleDescription *avc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sample_description))
    {
      AP4_Array<AP4_DataBuffer>& buffer = avc->GetPictureParameters();
      AP4_AvcPictureParameterSet pps;
      for (unsigned int i(0); i < buffer.ItemCount(); ++i)
      {
        if (AP4_SUCCEEDED(AP4_AvcFrameParser::ParsePPS(buffer[i].GetData(), buffer[i].GetDataSize(), pps)) && pps.pic_parameter_set_id == pictureId)
        {
          buffer = avc->GetSequenceParameters();
          AP4_AvcSequenceParameterSet sps;
          for (unsigned int i(0); i < buffer.ItemCount(); ++i)
          {
            if (AP4_SUCCEEDED(AP4_AvcFrameParser::ParseSPS(buffer[i].GetData(), buffer[i].GetDataSize(), sps)) && sps.seq_parameter_set_id == pps.seq_parameter_set_id)
            {
              sps.GetInfo(width, height);
              return true;
            }
          }
          break;
        }
      }
    }
    return false;
  };
};

/***********************   HEVC   ************************/

class HEVCCodecHandler : public CodecHandler
{
public:
  HEVCCodecHandler(AP4_SampleDescription *sd)
    :CodecHandler(sd)
  {
    if (AP4_HevcSampleDescription *hevc = AP4_DYNAMIC_CAST(AP4_HevcSampleDescription, sample_description))
    {
      extra_data_size = hevc->GetRawBytes().GetDataSize();
      extra_data = hevc->GetRawBytes().GetData();
      naluLengthSize = hevc->GetNaluLengthSize();
    }
  }
};

/***********************   MPEG   ************************/

class MPEGCodecHandler : public CodecHandler
{
public:
  MPEGCodecHandler(AP4_SampleDescription *sd)
    :CodecHandler(sd)
  {
    if (AP4_MpegSampleDescription *aac = AP4_DYNAMIC_CAST(AP4_MpegSampleDescription, sample_description))
    {
      extra_data_size = aac->GetDecoderInfo().GetDataSize();
      extra_data = aac->GetDecoderInfo().GetData();
    }
  }
};


/*******************************************************
|   FragmentedSampleReader
********************************************************/
class FragmentedSampleReader : public AP4_LinearReader
{
public:

  FragmentedSampleReader(AP4_ByteStream *input, AP4_Movie *movie, AP4_Track *track,
    AP4_UI32 streamId, AP4_CencSingleSampleDecrypter *ssd)
    : AP4_LinearReader(*movie, input)
    , m_Track(track)
    , m_dts(0.0)
    , m_pts(0.0)
    , m_eos(false)
    , m_StreamId(streamId)
    , m_SingleSampleDecryptor(ssd)
    , m_Decrypter(0)
    , m_Protected_desc(0)
    , m_codecHandler(0)
  {
    EnableTrack(m_Track->GetId());

    AP4_SampleDescription *desc(m_Track->GetSampleDescription(0));
    if (desc->GetType() == AP4_SampleDescription::TYPE_PROTECTED)
      m_Protected_desc = static_cast<AP4_ProtectedSampleDescription*>(desc);
    switch (desc->GetFormat())
    {
    case AP4_SAMPLE_FORMAT_AVC1:
    case AP4_SAMPLE_FORMAT_AVC2:
    case AP4_SAMPLE_FORMAT_AVC3:
    case AP4_SAMPLE_FORMAT_AVC4:
      m_codecHandler = new AVCCodecHandler(desc);
      break;
    case AP4_SAMPLE_FORMAT_HEV1:
    case AP4_SAMPLE_FORMAT_HVC1:
      m_codecHandler = new HEVCCodecHandler(desc);
      break;
    case AP4_SAMPLE_FORMAT_MP4A:
      m_codecHandler = new MPEGCodecHandler(desc);
      break;
    default:
      m_codecHandler = new CodecHandler(desc);
      break;
    }
  }

  ~FragmentedSampleReader()
  {
    delete m_Decrypter;
    delete m_codecHandler;
  }

  AP4_Result ReadSample()
  {
    AP4_Result result;
    if (AP4_FAILED(result = ReadNextSample(m_Track->GetId(), m_sample_, m_Decrypter ? m_encrypted : m_sample_data_)))
    {
      if (result == AP4_ERROR_EOS) {
        m_eos = true;
      }
      else {
        return result;
      }
    }
    if (m_Decrypter && AP4_FAILED(result = m_Decrypter->DecryptSampleData(m_encrypted, m_sample_data_, NULL)))
    {
      xbmc->Log(ADDON::LOG_ERROR, "Decrypt Sample returns failure!");
      return result;
    }

    m_dts = (double)m_sample_.GetDts() / (double)m_Track->GetMediaTimeScale();
    m_pts = (double)m_sample_.GetCts() / (double)m_Track->GetMediaTimeScale();

    m_codecHandler->UpdatePPSId(m_sample_data_);

    return AP4_SUCCESS;
  };

  void Reset(bool bEOS)
  {
    AP4_LinearReader::Reset();
    m_eos = bEOS;
  }

  bool EOS()const{ return m_eos; };
  double DTS()const{ return m_dts; };
  double PTS()const{ return m_pts; };
  const AP4_Sample &Sample()const { return m_sample_; };
  AP4_UI32 GetStreamId()const{ return m_StreamId; };
  AP4_Size GetSampleDataSize()const{ return m_sample_data_.GetDataSize(); };
  const AP4_Byte *GetSampleData()const{ return m_sample_data_.GetData(); };
  double GetDuration()const{ return (double)m_sample_.GetDuration() / (double)m_Track->GetMediaTimeScale(); };
  const AP4_UI08 *GetExtraData(){ return m_codecHandler->extra_data; };
  AP4_Size GetExtraDataSize(){ return m_codecHandler->extra_data_size; };
  bool GetVideoInformation(unsigned int &width, unsigned int &height){ return  m_codecHandler->GetVideoInformation(width, height); };
  bool TimeSeek(double pts)
  {
    bool result;
    while ((result = AP4_SUCCEEDED(ReadSample())) && m_pts < pts);
    return result;
  };

protected:
  virtual AP4_Result ProcessMoof(AP4_ContainerAtom* moof,
    AP4_Position       moof_offset,
    AP4_Position       mdat_payload_offset)
  {
    if (m_Protected_desc)
    {
      //Setup the decryption
      AP4_Result result;
      AP4_CencSampleInfoTable *sample_table;
      AP4_UI32 algorithm_id = 0;

      delete m_Decrypter;
      m_Decrypter = 0;

      AP4_ContainerAtom *traf = AP4_DYNAMIC_CAST(AP4_ContainerAtom, moof->GetChild(AP4_ATOM_TYPE_TRAF, 0));

      if (!m_Protected_desc || !traf)
        return AP4_ERROR_INVALID_FORMAT;

      if (AP4_FAILED(result = AP4_CencSampleInfoTable::Create(m_Protected_desc, traf, algorithm_id, *m_FragmentStream, moof_offset, sample_table)))
        return result;

      if (AP4_FAILED(result = AP4_CencSampleDecrypter::Create(sample_table, algorithm_id, 0, 0, 0, m_SingleSampleDecryptor, m_Decrypter)))
        return result;
    }
    return AP4_LinearReader::ProcessMoof(moof, moof_offset, mdat_payload_offset);
  }

private:
  AP4_Track *m_Track;
  AP4_UI32 m_StreamId;
  bool m_eos;
  double m_dts, m_pts;

  AP4_Sample     m_sample_;
  AP4_DataBuffer m_encrypted, m_sample_data_;

  CodecHandler *m_codecHandler;

  AP4_ProtectedSampleDescription *m_Protected_desc;
  AP4_CencSingleSampleDecrypter *m_SingleSampleDecryptor;
  AP4_CencSampleDecrypter *m_Decrypter;
};

/*******************************************************
Main class Session
********************************************************/
Session *session = 0;

void Session::STREAM::disable()
{
  if (enabled)
  {
    stream_.stop();
    SAFE_DELETE(reader_);
    SAFE_DELETE(input_file_);
    SAFE_DELETE(input_);
    enabled = false;
  }
}

Session::Session(const char *strURL)
  :single_sample_decryptor_(0)
  , mpdFileURL_(strURL)
  , width_(1280)
  , height_(720)
  , language_("de")
  , fixed_bandwidth_(10000000)
  , last_pts_(0)
{
}

Session::~Session()
{
  for (std::vector<STREAM*>::iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    SAFE_DELETE(*b);
  streams_.clear();
}

/*----------------------------------------------------------------------
|   initialize
+---------------------------------------------------------------------*/

bool Session::initialize()
{
  // Get URN's wich are supported by this addon
  GetSupportedDecrypterURN(license_type_, license_key_, dashtree_.adp_pssh_);

  // Open mpd file
  const char* delim(strrchr(mpdFileURL_.c_str(), '/'));
  if (!delim)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Invalid mpdURL: / expected (%s)", mpdFileURL_.c_str());
    return false;
  }
  dashtree_.base_url_ = std::string(mpdFileURL_.c_str(), (delim - mpdFileURL_.c_str()) + 1);

  if (!dashtree_.open(mpdFileURL_.c_str()) || dashtree_.empty())
  {
    xbmc->Log(ADDON::LOG_ERROR, "Could not open / parse mpdURL (%s)", mpdFileURL_.c_str());
    return false;
  }
  xbmc->Log(ADDON::LOG_INFO, "Successfully parsed .mpd file. Download speed: %0.4f Bytes/s", dashtree_.download_speed_);

  if (dashtree_.encryptionState_ == dash::DASHTree::ENCRYTIONSTATE_ENCRYPTED)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Unable to handle decryption. Unsupported!");
    return false;
  }

  // create SESSION::STREAM objects. One for each AdaptationSet
  unsigned int i(0);
  const dash::DASHTree::AdaptationSet *adp;

  for (std::vector<STREAM*>::iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    SAFE_DELETE(*b);
  streams_.clear();

  while (adp = dashtree_.GetAdaptationSet(i++))
  {
    streams_.push_back(new STREAM(dashtree_, adp->type_));
    STREAM &stream(*streams_.back());
    stream.stream_.prepare_stream(adp, width_, height_, language_.c_str(), fixed_bandwidth_);

    const dash::DASHTree::Representation *rep(stream.stream_.getRepresentation());

    stream.info_.m_Width = rep->width_;
    stream.info_.m_Height = rep->height_;
    stream.info_.m_Aspect = rep->aspect_;
    stream.info_.m_pID = i;
    switch (adp->type_)
    {
    case dash::DASHTree::VIDEO:
      stream.info_.m_streamType = INPUTSTREAM_INFO::TYPE_VIDEO;
      break;
    case dash::DASHTree::AUDIO:
      stream.info_.m_streamType = INPUTSTREAM_INFO::TYPE_AUDIO;
      break;
    case dash::DASHTree::TEXT:
      stream.info_.m_streamType = INPUTSTREAM_INFO::TYPE_TELETEXT;
      break;
    }

    if (rep->codecs_.find("mp4a") == 0)
      strcpy(stream.info_.m_codecName, "aac");
    else if (rep->codecs_.find("ec-3") == 0 || rep->codecs_.find("ac-3") == 0)
      strcpy(stream.info_.m_codecName, "eac3");
    else if (rep->codecs_.find("avc") == 0)
      strcpy(stream.info_.m_codecName, "h264");
    else if (rep->codecs_.find("hevc") == 0)
      strcpy(stream.info_.m_codecName, "hevc");

    stream.info_.m_FpsRate = rep->fpsRate_;
    stream.info_.m_FpsScale = rep->fpsScale_;
    stream.info_.m_SampleRate = rep->samplingRate_;
    strcpy(stream.info_.m_language, adp->language_.c_str());
  }
  
  // Try to initialize an SingleSampleDecryptor
  if (dashtree_.encryptionState_)
  {
    AP4_DataBuffer init_data;

    if (dashtree_.adp_pssh_.second == "FILE")
    {
      std::string strkey(dashtree_.adp_pssh_.first.substr(9));
      while (size_t pos = strkey.find('-') != std::string::npos)
        strkey.erase(pos, 1);
      if (strkey.size() != 32)
      {
        xbmc->Log(ADDON::LOG_ERROR, "Key system mismatch (%s)!", dashtree_.adp_pssh_.first.c_str());
        return false;
      }

      unsigned char key_system[16];
      AP4_ParseHex(strkey.c_str(), key_system, 16);

      Session::STREAM *stream(streams_[0]);

      stream->enabled = true;
      stream->stream_.start_stream(0);
      stream->stream_.select_stream(true);

      stream->input_ = new AP4_DASHStream(&stream->stream_);
      stream->input_file_ = new AP4_File(*stream->input_, AP4_DefaultAtomFactory::Instance, true);
      AP4_Movie* movie = stream->input_file_->GetMovie();
      if (movie == NULL)
      {
        xbmc->Log(ADDON::LOG_ERROR, "No MOOV in stream!");
        stream->disable();
        return false;
      }
      AP4_Array<AP4_PsshAtom*>& pssh = movie->GetPsshAtoms();

      for (unsigned int i = 0; !init_data.GetDataSize() && i < pssh.ItemCount(); i++)
      {
        if (memcmp(pssh[i]->GetSystemId(), key_system, 16) == 0)
          init_data.AppendData(pssh[i]->GetData().GetData(), pssh[i]->GetData().GetDataSize());
      }

      if (!init_data.GetDataSize())
      {
        xbmc->Log(ADDON::LOG_ERROR, "Could not extract license from video stream (PSSH not found)");
        stream->disable();
        return false;
      }
      stream->disable();
    }
    else
    {
      init_data.SetBufferSize(1024);
      size_t init_data_size(1024);
      b64_decode(dashtree_.pssh_.second.data(), dashtree_.pssh_.second.size(), init_data.UseData(), init_data_size);
      init_data.SetDataSize(init_data_size);
    }
    return CreateSingleSampleDecrypter(init_data);
  }
  return true;
}

FragmentedSampleReader *Session::GetNextSample()
{
  FragmentedSampleReader *res(0);
  for (std::vector<STREAM*>::const_iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    if ((*b)->enabled && !(*b)->reader_->EOS()
    && (!res || (*b)->reader_->DTS() < res->DTS()))
        res = (*b)->reader_;

  if (res)
    last_pts_ = res->PTS();

  return res;
}

bool Session::SeekTime(double seekTime)
{
  bool ret(false);

  for (std::vector<STREAM*>::const_iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    if ((*b)->enabled)
    {
      bool bReset;
      if ((*b)->stream_.seek_time(seekTime, last_pts_, bReset))
      {
        if (bReset)
          (*b)->reader_->Reset(false);
        if (!(*b)->reader_->TimeSeek(seekTime))
          (*b)->reader_->Reset(true);
        else
          ret = true;
      }
      else
        (*b)->reader_->Reset(true);
    }
  return ret;
}

/***************************  Interface *********************************/

#include "kodi_inputstream_dll.h"
#include "libKODI_inputstream.h"

extern "C" {

  ADDON_STATUS curAddonStatus = ADDON_STATUS_UNKNOWN;
  CHelper_libKODI_inputstream *ipsh = 0;

  /***********************************************************
  * Standard AddOn related public library functions
  ***********************************************************/

  ADDON_STATUS ADDON_Create(void* hdl, void* props)
  {
    // initialize globals
    session = nullptr;

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

    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_Create()");

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
    xbmc->Log(ADDON::LOG_DEBUG, "ADDON_Destroy()");
    SAFE_DELETE(session);
    SAFE_DELETE(xbmc);
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

    session = new Session(props.m_strURL);
    if (!session->initialize())
    {
      SAFE_DELETE(session);
      return false;
    }
    return true;
  }

  void Close(void)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "Close()");
    SAFE_DELETE(session);
  }

  const char* GetPathList(void)
  {
    static std::string strSettings;
    strSettings.clear();

    char buffer[1024];

    unsigned int nURL(0);
    while (1)
    {
      sprintf(buffer, "URL%d", ++nURL);
      if (xbmc->GetSetting(buffer, buffer))
      {
        if (buffer[0])
        {
          if (!strSettings.empty())
            strSettings += "|";
          strSettings += buffer;
        }
      }
      else
        break;
    }
    return strSettings.c_str();
  }

  struct INPUTSTREAM_IDS GetStreamIds()
  {
    xbmc->Log(ADDON::LOG_DEBUG, "GetStreamIds()");
    INPUTSTREAM_IDS iids;
    
    if(session)
    {
        iids.m_streamCount = session->GetStreamCount();
        for (unsigned int i(0); i < iids.m_streamCount;++i)
          iids.m_streamIds[i] = i+1;
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
    caps.m_supportsPause = true;
    return caps;
  }

  struct INPUTSTREAM_INFO GetStream(int streamid)
  {
    static struct INPUTSTREAM_INFO dummy_info = {
      INPUTSTREAM_INFO::TYPE_NONE, "", 0, 0, 0, "",
      0, 0, 0, 0, 0.0f,
      0, 0, 0, 0, 0 };

    xbmc->Log(ADDON::LOG_DEBUG, "GetStream(%d)", streamid);

    Session::STREAM *stream(session->GetStream(streamid));
    if (stream)
      return stream->info_;

    return dummy_info;
  }

  void EnableStream(int streamid, bool enable)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "EnableStream(%d, %d)", streamid, (int)enable);

    if (!session)
      return;

    Session::STREAM *stream(session->GetStream(streamid));

    if (!stream)
      return;

    if (enable)
    {
      if (stream->enabled)
        return;

      stream->enabled = true;

      stream->stream_.start_stream(0);
      stream->stream_.select_stream(true);

      stream->input_ = new AP4_DASHStream(&stream->stream_);
      stream->input_file_ = new AP4_File(*stream->input_, AP4_DefaultAtomFactory::Instance, true);
      AP4_Movie* movie = stream->input_file_->GetMovie();
      if (movie == NULL)
      {
        xbmc->Log(ADDON::LOG_ERROR, "No MOOV in stream!");
        return stream->disable();
      }

      static const AP4_Track::Type TIDC[dash::DASHTree::STREAM_TYPE_COUNT] =
      { AP4_Track::TYPE_UNKNOWN, AP4_Track::TYPE_VIDEO, AP4_Track::TYPE_AUDIO, AP4_Track::TYPE_TEXT };

      AP4_Track *track = movie->GetTrack(TIDC[stream->stream_.get_type()]);
      if (!track)
      {
        xbmc->Log(ADDON::LOG_ERROR, "No suitable track found in stream");
        return stream->disable();
      }

      stream->reader_ = new FragmentedSampleReader(stream->input_, movie, track, streamid, session->GetSingleSampleDecryptor());

      // Jump to the segment at wich we want to continue
      // stream->stream_.start_stream(stream->current_segment_);

      if (!AP4_SUCCEEDED(stream->reader_->ReadSample()))
        return stream->disable();

      // Maybe we have changed information for hints after parsing the first packet...
      stream->reader_->GetVideoInformation(stream->info_.m_Width, stream->info_.m_Height);
      // ExtraData is now available......
      stream->info_.m_ExtraData = stream->reader_->GetExtraData();
      stream->info_.m_ExtraSize = stream->reader_->GetExtraDataSize();

      // Set the session Changed to force new GetStreamInfo call from kodi -> addon
      session->CheckChange(true);

      return;
    }
    return stream->disable();
  }

  int ReadStream(unsigned char*, unsigned int)
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

    if (session->CheckChange())
    {
      DemuxPacket *p = ipsh->AllocateDemuxPacket(0);
      p->iStreamId = DMX_SPECIALID_STREAMCHANGE;
      return p;
    }

    FragmentedSampleReader *sr(session->GetNextSample());

    if (sr)
    {
      const AP4_Sample &s(sr->Sample());
      DemuxPacket *p = ipsh->AllocateDemuxPacket(sr->GetSampleDataSize());
      p->dts = sr->DTS() * 1000000;
      p->dts = sr->PTS() * 1000000;
      p->duration = sr->GetDuration() * 1000000;
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
    if (!session)
      return false;

    xbmc->Log(ADDON::LOG_INFO, "DemuxSeekTime (%0.4f)", time);

    return session->SeekTime(static_cast<double>(time)*0.001f);
  }

  void DemuxSetSpeed(int speed)
  {

  }

  int GetTotalTime()
  {
    if (!session)
      return 0;

    return static_cast<int>(session->GetTotalTime()*1000);
  }

  int GetTime()
  {
    if (!session)
      return 0;

    return static_cast<int>(session->GetPTS() * 1000);
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
