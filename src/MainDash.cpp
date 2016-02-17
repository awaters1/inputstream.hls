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
#include <vector>

#include "dash/DASHTree.h"
#include "dash/DASHStream.h"

#include "xbmc_addon_types.h"
#include "libXBMC_addon.h"
#include "kodi_inputstream_types.h"

#include "Ap4.h"

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

class KodiDASHTree : public dash::DASHTree
{
protected:
  virtual bool download(const char* url)
  {
    // open the file
    std::string strURL(url);
    strURL += "|encoding%3Dgzip";
    
    void* file = xbmc->OpenFile(strURL.c_str(), 0);
    if (!file)
      return false;
  
    // read the file
    char buf[8192];
    size_t nbRead;
    while ((nbRead = xbmc->ReadFile(file, buf, 8192)) > 0 && write_data(buf, nbRead));

    xbmc->CloseFile(file);

    return nbRead == 0;
  };
};

class KodiDASHStream : public dash::DASHStream
{
public:
  KodiDASHStream(dash::DASHTree &tree, dash::DASHTree::StreamType type)
    :dash::DASHStream(tree, type) 
  {
  };
protected:
  virtual bool download(const char* url)
  {
    // open the file
    void* file = xbmc->OpenFile(url, 0);
    if (!file)
      return false;

    // read the file
    char *buf = (char*)malloc(1024*1024);
    size_t nbRead;
    while ((nbRead = xbmc->ReadFile(file, buf, 1024 * 1024)) > 0 && write_data(buf, nbRead));
    free(buf);

    xbmc->CloseFile(file);

    return nbRead == 0;
  };
};

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
        return AP4_AvcFrameParser::ReadGolomb(bits);
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
class Session
{
public:
  Session();
  ~Session();
  bool initialize();
  void SetStreamProperties(uint16_t width, uint16_t height, const char* language, uint32_t maxBitPS, bool allow_ec_3);
  FragmentedSampleReader *GetNextSample();

  struct STREAM
  {
    STREAM(dash::DASHTree t, dash::DASHTree::StreamType s) :stream_(t, s), enabled(false), current_segment_(0), input_(0), reader_(0), input_file_(0) { memset(&info_, 0, sizeof(info_)); };
    ~STREAM(){ disable(); };
    void disable()
    {
      if (enabled)
      {
        stream_.stop();
        SAFE_DELETE(reader_);
        SAFE_DELETE(input_file_);
        SAFE_DELETE(input_);
        enabled = false;
      }
    };

    bool enabled;
    uint32_t current_segment_;
    KodiDASHStream stream_;
    AP4_ByteStream *input_;
    AP4_File *input_file_;
    INPUTSTREAM_INFO info_;
    FragmentedSampleReader *reader_;
  };

  STREAM *GetStream(unsigned int sid) const { return sid - 1 < streams_.size() ? streams_[sid - 1] : 0; };
  unsigned int GetStreamCount() const { return streams_.size(); };
  AP4_CencSingleSampleDecrypter * GetSingleSampleDecryptor()const{ return single_sample_decryptor_; };
  int GetTotalTime()const { return (int)dashtree_.overallSeconds_; };
  bool CheckChange(bool bSet = false){ bool ret = changed_; changed_ = bSet; return ret; };

private:
  std::string mpdFileURL_;

  KodiDASHTree dashtree_;

  std::vector<STREAM*> streams_;

  uint16_t width_, height_;
  std::string language_;
  uint32_t fixed_bandwidth_;
  bool changed_;

  AP4_CencSingleSampleDecrypter *single_sample_decryptor_;
} *session = 0;

Session::Session()
  :single_sample_decryptor_(0)
  , width_(1280)
  , height_(720)
  , language_("de")
  , fixed_bandwidth_(10000000)
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
  mpdFileURL_ = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live/mp4-live-mpd-AV-BS.mpd";

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
  return true;
}

FragmentedSampleReader *Session::GetNextSample()
{
  FragmentedSampleReader *res(0);
  for (std::vector<STREAM*>::const_iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    if ((*b)->enabled && !(*b)->reader_->EOS()
    && (!res || (*b)->reader_->DTS() < res->DTS()))
        res = (*b)->reader_;

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
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: OpenStream()");

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
    iids.m_streamCount = session->GetStreamCount();
    for (unsigned int i(0); i < iids.m_streamCount;++i)
      iids.m_streamIds[i] = i+1;
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
      INPUTSTREAM_INFO::TYPE_NONE, "", 0, 0, 0, "",
      0, 0, 0, 0, 0.0f,
      0, 0, 0, 0, 0 };

    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: GetStream(%d)", streamid);

    Session::STREAM *stream(session->GetStream(streamid));
    if (stream)
      return stream->info_;

    return dummy_info;
  }

  void EnableStream(int streamid, bool enable)
  {
    xbmc->Log(ADDON::LOG_DEBUG, "InputStream.mpd: EnableStream(%d, %d)", streamid, (int)enable);

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

      stream->stream_.start_stream(stream->current_segment_);
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

      if (!AP4_SUCCEEDED(stream->reader_->ReadSample()))
        return stream->disable();

      // Maybe we have changed information for hints after parsing the first packet...
      stream->reader_->GetVideoInformation(stream->info_.m_Width, stream->info_.m_Height);
      // ExtraData is now available......
      stream->info_.m_ExtraData = stream->reader_->GetExtraData();
      stream->info_.m_ExtraSize = stream->reader_->GetExtraDataSize();

      // Set the session Changed to force new streamInfor call from kodi -> addon
      session->CheckChange(true);

      return;
    }
    return stream->disable();
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
    return false;
  }

  void DemuxSetSpeed(int speed)
  {

  }

  int GetTotalTime()
  {
    if (!session)
      return 0;

    return session->GetTotalTime();
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
