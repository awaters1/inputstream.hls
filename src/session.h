/*
*      Copyright (C) 2016-2016 peak3d
*      http://www.peak3d.de
*      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
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
#pragma once

#include <cfloat>

#include "decoding.h"
#include "KodiDASH.h"
#include "kodi_inputstream_types.h"
#include "SSD_dll.h"


class Session: public FragmentObserver
{
public:
  Session(const char *strURL, const char *strLicType, const char* strLicKey, const char* strLicData, const char* profile_path);
  ~Session();
  bool initialize();
  FragmentedSampleReader *GetNextSample();

  struct STREAM
  {
    STREAM(dash::DASHTree &t, dash::DASHTree::StreamType s) :stream_(t, s), enabled(false), current_segment_(0), input_(0), reader_(0), input_file_(0) { memset(&info_, 0, sizeof(info_)); };
    ~STREAM() { disable(); free((void*)info_.m_ExtraData); };
    void disable();

    bool enabled;
    uint32_t current_segment_;
    KodiDASHStream stream_;
    AP4_ByteStream *input_;
    AP4_File *input_file_;
    INPUTSTREAM_INFO info_;
    FragmentedSampleReader *reader_;
  };

  void UpdateStream(STREAM &stream);

  STREAM *GetStream(unsigned int sid)const { return sid - 1 < streams_.size() ? streams_[sid - 1] : 0; };
  unsigned int GetStreamCount() const { return streams_.size(); };
  uint8_t getMediaTypeMask() const { return media_type_mask_; };
  std::uint16_t GetWidth()const { return width_; };
  std::uint16_t GetHeight()const { return height_; };
  AP4_CencSingleSampleDecrypter * GetSingleSampleDecryptor()const{ return single_sample_decryptor_; };
  double GetPresentationTimeOffset() { return dashtree_.minPresentationOffset < DBL_MAX? dashtree_.minPresentationOffset:0; };
  double GetTotalTime()const { return dashtree_.overallSeconds_; };
  double GetPTS()const { return last_pts_; };
  bool CheckChange(bool bSet = false){ bool ret = changed_; changed_ = bSet; return ret; };
  void SetVideoResolution(unsigned int w, unsigned int h) { width_ = w < maxwidth_ ? w : maxwidth_; height_ = h < maxheight_ ? h : maxheight_;};
  bool SeekTime(double seekTime, unsigned int streamId = 0, bool preceeding=true);
  bool IsLive() const { return dashtree_.has_timeshift_buffer_; };

  //Observer Section
  void BeginFragment(AP4_UI32 streamId) override;
  void EndFragment(AP4_UI32 streamId) override;

protected:
  void GetSupportedDecrypterURN(std::pair<std::string, std::string> &urn);
  AP4_CencSingleSampleDecrypter *CreateSingleSampleDecrypter(AP4_DataBuffer &streamCodec);

private:
  std::string mpdFileURL_;
  std::string license_key_, license_type_, license_data_;
  std::string profile_path_;
  void * decrypterModule_;
  SSD_DECRYPTER *decrypter_;

  KodiDASHTree dashtree_;

  std::vector<STREAM*> streams_;

  uint16_t width_, height_;
  uint16_t maxwidth_, maxheight_;
  uint32_t fixed_bandwidth_;
  bool changed_;
  bool manual_streams_;
  double last_pts_;
  uint8_t media_type_mask_;

  AP4_CencSingleSampleDecrypter *single_sample_decryptor_;
};

