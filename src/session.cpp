/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "globals.h"
#include "kodi.h"

#include "session.h"
#include "helpers.h"
#include "kodi_vfs_types.h"

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

Session::Session(const char *strURL, const char *strLicType, const char* strLicKey, const char* strLicData, const char* profile_path)
  :single_sample_decryptor_(0)
  , mpdFileURL_(strURL)
  , license_type_(strLicType)
  , license_key_(strLicKey)
  , license_data_(strLicData)
  , profile_path_(profile_path)
  , width_(kodiDisplayWidth)
  , height_(kodiDisplayHeight)
  , last_pts_(0)
  , decrypterModule_(0)
  , decrypter_(0)
  , changed_(false)
  , manual_streams_(false)
{
  std::string fn(profile_path_ + "bandwidth.bin");
  FILE* f = fopen(fn.c_str(), "rb");
  if (f)
  {
    double val;
    fread(&val, sizeof(double), 1, f);
    dashtree_.bandwidth_ = static_cast<uint32_t>(val * 8);
    dashtree_.set_download_speed(val);
    fclose(f);
  }
  else
    dashtree_.bandwidth_ = 4000000;
  xbmc->Log(ADDON::LOG_DEBUG, "Initial bandwidth: %u ", dashtree_.bandwidth_);

  int buf;
  xbmc->GetSetting("MAXRESOLUTION", (char*)&buf);
  xbmc->Log(ADDON::LOG_DEBUG, "MAXRESOLUTION selected: %d ", buf);
  switch (buf)
  {
  case 0:
    maxwidth_ = 0xFFFF;
    maxheight_ = 0xFFFF;
    break;
  case 2:
    maxwidth_ = 1920;
    maxheight_ = 1080;
    break;
  default:
    maxwidth_ = 1280;
    maxheight_ = 720;
  }
  if (width_ > maxwidth_)
    width_ = maxwidth_;

  if (height_ > maxheight_)
    height_ = maxheight_;

  xbmc->GetSetting("STREAMSELECTION", (char*)&buf);
  xbmc->Log(ADDON::LOG_DEBUG, "STREAMSELECTION selected: %d ", buf);
  manual_streams_ = buf != 0;

  xbmc->GetSetting("MEDIATYPE", (char*)&buf);
  switch (buf)
  {
  case 1:
    media_type_mask_ = static_cast<uint8_t>(1U) << dash::DASHTree::AUDIO;
    break;
  case 2:
    media_type_mask_ = static_cast<uint8_t>(1U) << dash::DASHTree::VIDEO;
    break;
  default:
    media_type_mask_ = static_cast<uint8_t>(~0);
  }
}

Session::~Session()
{
  for (std::vector<STREAM*>::iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    SAFE_DELETE(*b);
  streams_.clear();

  if (decrypterModule_)
  {
    dlclose(decrypterModule_);
    decrypterModule_ = 0;
    decrypter_ = 0;
  }

  std::string fn(profile_path_ + "bandwidth.bin");
  FILE* f = fopen(fn.c_str(), "wb");
  if (f)
  {
    double val(dashtree_.get_average_download_speed());
    fwrite((const char*)&val, sizeof(double), 1, f);
    fclose(f);
  }
}

void Session::GetSupportedDecrypterURN(std::pair<std::string, std::string> &urn)
{
  typedef SSD_DECRYPTER *(*CreateDecryptorInstanceFunc)(SSD_HOST *host, uint32_t version);

  char specialpath[1024];
  if (!xbmc->GetSetting("DECRYPTERPATH", specialpath))
  {
    xbmc->Log(ADDON::LOG_DEBUG, "DECRYPTERPATH not specified in settings.xml");
    return;
  }
  addonstring path(xbmc->TranslateSpecialProtocol(specialpath));

  kodihost.SetLibraryPath(path.c_str());

  VFSDirEntry *items(0);
  unsigned int num_items(0);

  xbmc->Log(ADDON::LOG_DEBUG, "Searching for decrypters in: %s", path.c_str());

  if (!xbmc->GetDirectory(path.c_str(), "", &items, &num_items))
    return;

  for (unsigned int i(0); i < num_items; ++i)
  {
    if (strncmp(items[i].label, "ssd_", 4) && strncmp(items[i].label, "libssd_", 7))
      continue;

    void * mod(dlopen(items[i].path, RTLD_LAZY));
    if (mod)
    {
      CreateDecryptorInstanceFunc startup;
      if ((startup = (CreateDecryptorInstanceFunc)dlsym(mod, "CreateDecryptorInstance")))
      {
        SSD_DECRYPTER *decrypter = startup(&kodihost, SSD_HOST::version);
        const char *suppUrn(0);

        if (decrypter && (suppUrn = decrypter->Supported(license_type_.c_str(), license_key_.c_str())))
        {
          xbmc->Log(ADDON::LOG_DEBUG, "Found decrypter: %s", items[i].path);
          decrypterModule_ = mod;
          decrypter_ = decrypter;
          urn.first = suppUrn;
          break;
        }
      }
      dlclose(mod);
    }
    else
    {
      xbmc->Log(ADDON::LOG_DEBUG, "%s", dlerror());
    }
  }
  xbmc->FreeDirectory(items, num_items);
}

AP4_CencSingleSampleDecrypter *Session::CreateSingleSampleDecrypter(AP4_DataBuffer &streamCodec)
{
  if (decrypter_)
    return decrypter_->CreateSingleSampleDecrypter(streamCodec);
  return 0;
};

/*----------------------------------------------------------------------
|   initialize
+---------------------------------------------------------------------*/

bool Session::initialize()
{
  // Get URN's wich are supported by this addon
  if (!license_type_.empty())
  {
    GetSupportedDecrypterURN(dashtree_.adp_pssh_);
    xbmc->Log(ADDON::LOG_DEBUG, "Supported URN: %s", dashtree_.adp_pssh_.first.c_str());
  }

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
  xbmc->Log(ADDON::LOG_INFO, "Successfully parsed .mpd file. #Streams: %d Download speed: %0.4f Bytes/s", dashtree_.periods_[0]->adaptationSets_.size(), dashtree_.download_speed_);

  if (dashtree_.encryptionState_ == dash::DASHTree::ENCRYTIONSTATE_ENCRYPTED)
  {
    xbmc->Log(ADDON::LOG_ERROR, "Unable to handle decryption. Unsupported!");
    return false;
  }

  uint32_t min_bandwidth(0), max_bandwidth(0);
  {
    int buf;
    xbmc->GetSetting("MINBANDWIDTH", (char*)&buf); min_bandwidth = buf;
    xbmc->GetSetting("MAXBANDWIDTH", (char*)&buf); max_bandwidth = buf;
  }

  // create SESSION::STREAM objects. One for each AdaptationSet
  unsigned int i(0);
  const dash::DASHTree::AdaptationSet *adp;

  for (std::vector<STREAM*>::iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    SAFE_DELETE(*b);
  streams_.clear();

  while ((adp = dashtree_.GetAdaptationSet(i++)))
  {
    size_t repId = manual_streams_ ? adp->repesentations_.size() : 0;

    do {
      streams_.push_back(new STREAM(dashtree_, adp->type_));
      STREAM &stream(*streams_.back());
      stream.stream_.prepare_stream(adp, width_, height_, min_bandwidth, max_bandwidth, repId);

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
      default:
        break;
      }
      stream.info_.m_pID = i | (repId << 16);
      strcpy(stream.info_.m_language, adp->language_.c_str());
      stream.info_.m_ExtraData = nullptr;
      stream.info_.m_ExtraSize = 0;

      UpdateStream(stream);

    } while (repId--);
  }

  // Try to initialize an SingleSampleDecryptor
  if (dashtree_.encryptionState_)
  {
    AP4_DataBuffer init_data;

    if (dashtree_.pssh_.second == "FILE")
    {
      if (license_data_.empty())
      {
        std::string strkey(dashtree_.adp_pssh_.first.substr(9));
        size_t pos;
        while ((pos = strkey.find('-')) != std::string::npos)
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
        stream->stream_.start_stream(0, width_, height_);
        stream->stream_.select_stream(true, false, stream->info_.m_pID >> 16);

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
      else if (!dashtree_.defaultKID_.empty())
      {
        init_data.SetDataSize(16);
        AP4_Byte *data(init_data.UseData());
        const char *src(dashtree_.defaultKID_.c_str());
        AP4_ParseHex(src, data, 4);
        AP4_ParseHex(src + 9, data + 4, 2);
        AP4_ParseHex(src + 14, data + 6, 2);
        AP4_ParseHex(src + 19, data + 8, 2);
        AP4_ParseHex(src + 24, data + 10, 6);

        uint8_t ld[1024];
        unsigned int ld_size(1014);
        b64_decode(license_data_.c_str(), license_data_.size(), ld, ld_size);

        uint8_t *uuid((uint8_t*)strstr((const char*)ld, "{KID}"));
        if (uuid)
        {
          memmove(uuid + 11, uuid, ld_size - (uuid - ld));
          memcpy(uuid, init_data.GetData(), init_data.GetDataSize());
          init_data.SetData(ld, ld_size + 11);
        }
        else
          init_data.SetData(ld, ld_size);
      }
      else
        return false;
    }
    else
    {
      init_data.SetBufferSize(1024);
      unsigned int init_data_size(1024);
      b64_decode(dashtree_.pssh_.second.data(), dashtree_.pssh_.second.size(), init_data.UseData(), init_data_size);
      init_data.SetDataSize(init_data_size);
    }
    return (single_sample_decryptor_ = CreateSingleSampleDecrypter(init_data))!=0;
  }
  return true;
}

void Session::UpdateStream(STREAM &stream)
{
  const dash::DASHTree::Representation *rep(stream.stream_.getRepresentation());

  stream.info_.m_Width = rep->width_;
  stream.info_.m_Height = rep->height_;
  stream.info_.m_Aspect = rep->aspect_;

  if (!stream.info_.m_ExtraSize && rep->codec_private_data_.size())
  {
    stream.info_.m_ExtraSize = rep->codec_private_data_.size();
    stream.info_.m_ExtraData = (const uint8_t*)malloc(stream.info_.m_ExtraSize);
    memcpy((void*)stream.info_.m_ExtraData, rep->codec_private_data_.data(), stream.info_.m_ExtraSize);
  }

  // we currently use only the first track!
  std::string::size_type pos = rep->codecs_.find(",");
  if (pos == std::string::npos)
    pos = rep->codecs_.size();

  strncpy(stream.info_.m_codecInternalName, rep->codecs_.c_str(), pos);
  stream.info_.m_codecInternalName[pos] = 0;

  if (rep->codecs_.find("mp4a") == 0)
    strcpy(stream.info_.m_codecName, "aac");
  else if (rep->codecs_.find("ec-3") == 0 || rep->codecs_.find("ac-3") == 0)
    strcpy(stream.info_.m_codecName, "eac3");
  else if (rep->codecs_.find("avc") == 0)
    strcpy(stream.info_.m_codecName, "h264");
  else if (rep->codecs_.find("hevc") == 0 || rep->codecs_.find("hvc") == 0)
    strcpy(stream.info_.m_codecName, "hevc");
  else if (rep->codecs_.find("vp9") == 0)
    strcpy(stream.info_.m_codecName, "vp9");
  else if (rep->codecs_.find("opus") == 0)
    strcpy(stream.info_.m_codecName, "opus");
  else if (rep->codecs_.find("vorbis") == 0)
    strcpy(stream.info_.m_codecName, "vorbis");

  stream.info_.m_FpsRate = rep->fpsRate_;
  stream.info_.m_FpsScale = rep->fpsScale_;
  stream.info_.m_SampleRate = rep->samplingRate_;
  stream.info_.m_Channels = rep->channelCount_;
  stream.info_.m_Bandwidth = rep->bandwidth_;
}

FragmentedSampleReader *Session::GetNextSample()
{
  STREAM *res(0);
  for (std::vector<STREAM*>::const_iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
  {
    bool bStarted(false);
    if ((*b)->enabled && !(*b)->reader_->EOS() && AP4_SUCCEEDED((*b)->reader_->Start(bStarted))
      && (!res || (*b)->reader_->DTS() < res->reader_->DTS()))
      res = *b;

    if (bStarted && ((*b)->reader_->GetInformation((*b)->info_)))
      changed_ = true;
  }

  if (res)
  {
    if (res->reader_->GetInformation(res->info_))
      changed_ = true;
    last_pts_ = res->reader_->PTS();
    return res->reader_;
  }
  return 0;
}

bool Session::SeekTime(double seekTime, unsigned int streamId, bool preceeding)
{
  bool ret(false);

  //we don't have pts < 0 here and work internally with uint64
  if (seekTime < 0)
    seekTime = 0;

  for (std::vector<STREAM*>::const_iterator b(streams_.begin()), e(streams_.end()); b != e; ++b)
    if ((*b)->enabled && (streamId == 0 || (*b)->info_.m_pID == streamId))
    {
      bool bReset;
      if ((*b)->stream_.seek_time(seekTime + GetPresentationTimeOffset(), last_pts_, bReset))
      {
        if (bReset)
          (*b)->reader_->Reset(false);
        if (!(*b)->reader_->TimeSeek(seekTime, preceeding))
          (*b)->reader_->Reset(true);
        else
        {
          xbmc->Log(ADDON::LOG_INFO, "seekTime(%0.4f) for Stream:%d continues at %0.4f", seekTime, (*b)->info_.m_pID, (*b)->reader_->PTS());
          ret = true;
        }
      }
      else
        (*b)->reader_->Reset(true);
    }
  return ret;
}

void Session::BeginFragment(AP4_UI32 streamId)
{
  STREAM *s(streams_[streamId - 1]);
  s->reader_->SetPTSOffset(s->stream_.GetPTSOffset());
}

void Session::EndFragment(AP4_UI32 streamId)
{
  STREAM *s(streams_[streamId - 1]);
  dashtree_.SetFragmentDuration(
    s->stream_.getAdaptationSet(),
    s->stream_.getRepresentation(),
    s->stream_.getSegmentPos(),
    s->reader_->GetFragmentDuration(),
    s->reader_->GetTimeScale());
}
