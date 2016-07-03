/*
* DashStream.cpp
*****************************************************************************
* Copyright(C) 2015, liberty_developer
*
* Email: liberty.developer@xmail.net
*
* This source code and its use and distribution, is subject to the terms
* and conditions of the applicable license agreement.
*****************************************************************************/

#include "DASHStream.h"

#include <iostream>
#include <cstring>
#include "../oscompat.h"
#include <math.h>

using namespace dash;

DASHStream::DASHStream(DASHTree &tree, DASHTree::StreamType type)
  :tree_(tree)
  , type_(type)
  , observer_(0)
  , current_period_(tree_.periods_.empty() ? 0 : tree_.periods_[0])
  , current_adp_(0)
  , current_rep_(0)
{
}

bool DASHStream::download_segment()
{
  segment_buffer_.clear();
  absolute_position_ = 0;
  segment_read_pos_ = 0;

  if (!current_seg_)
    return false;

  std::string strURL;
  char rangebuf[128], *rangeHeader(0);

  if (!(current_rep_->flags_ & DASHTree::Representation::INDEXRANGEEXACT))
  {
    if (!(current_rep_->flags_ & DASHTree::Representation::TEMPLATE))
    {
      sprintf(rangebuf, "/range/%" PRIu64 "-%" PRIu64, current_seg_->range_begin_, current_seg_->range_end_);
      strURL = current_rep_->url_ + rangebuf;
      absolute_position_ = current_seg_->range_begin_;
    }
    else if (~current_seg_->range_end_) //templated segment
    {
      std::string media(current_adp_->segtpl_.media);
      if(!media.empty())
        media.replace(media.find("$RepresentationID$"), 18, current_rep_->id);
      else
        media = current_rep_->segtpl_.media;
      std::string::size_type lenReplace(7);
      std::string::size_type np(media.find("$Number"));
      if (np == std::string::npos)
      {
        lenReplace = 5;
        np = media.find("$Time");
      }
      np += lenReplace;

      std::string::size_type npe(media.find('$', np));

      char fmt[16];
      if (np == npe)
        strcpy(fmt, "%d");
      else
        strcpy(fmt, media.substr(np, npe - np).c_str());

      sprintf(rangebuf, fmt, static_cast<int>(current_seg_->range_end_));
      media.replace(np - lenReplace, npe - np + lenReplace + 1, rangebuf);
      strURL = media;
    }
    else //templated initialization segment
      strURL = current_rep_->url_;
  }
  else
  {
    strURL = current_rep_->url_;
    sprintf(rangebuf, "bytes=%" PRIu64 "-%" PRIu64, current_seg_->range_begin_, current_seg_->range_end_);
    rangeHeader = rangebuf;
  }

  return download(strURL.c_str(), rangeHeader);
}

bool DASHStream::write_data(const void *buffer, size_t buffer_size)
{
  segment_buffer_ += std::string((const char *)buffer, buffer_size);
  return true;
}

bool DASHStream::prepare_stream(const DASHTree::AdaptationSet *adp,
  const uint32_t width, const uint32_t height,
  uint32_t min_bandwidth, uint32_t max_bandwidth, unsigned int repId)
{
  width_ = type_ == DASHTree::VIDEO ? width : 0;
  height_ = type_ == DASHTree::VIDEO ? height : 0;

  uint32_t avg_bandwidth = tree_.bandwidth_;

  bandwidth_ = min_bandwidth;
  if (avg_bandwidth > bandwidth_)
    bandwidth_ = avg_bandwidth;
  if (max_bandwidth && bandwidth_ > max_bandwidth)
    bandwidth_ = max_bandwidth;

  stopped_ = false;

  bandwidth_ = static_cast<uint32_t>(bandwidth_ *(type_ == DASHTree::VIDEO ? 0.9 : 0.1));

  current_adp_ = adp;

  return select_stream(false, true, repId);
}

bool DASHStream::start_stream(const uint32_t seg_offset, uint16_t width, uint16_t height)
{
  segment_buffer_.clear();
  current_seg_ = current_rep_->get_segment(seg_offset);
  if (!current_seg_ || !current_rep_->get_next_segment(current_seg_))
  {
    absolute_position_ = ~0;
    stopped_ = true;
  }
  else
  {
    width_ = type_ == DASHTree::VIDEO ? width : 0;
    height_ = type_ == DASHTree::VIDEO ? height : 0;

    absolute_position_ = current_rep_->get_next_segment(current_seg_)->range_begin_;
    stopped_ = false;
  }
  return true;
}

uint32_t DASHStream::read(void* buffer, uint32_t  bytesToRead)
{
  if (stopped_)
    return 0;

  if (segment_read_pos_ >= segment_buffer_.size())
  {
    current_seg_ = current_rep_->get_next_segment(current_seg_);
    if (!download_segment() || segment_buffer_.empty())
      return 0;
  }
  if (bytesToRead)
  {
    uint32_t avail = segment_buffer_.size() - segment_read_pos_;
    if (avail > bytesToRead)
      avail = bytesToRead;
    memcpy(buffer, segment_buffer_.data() + segment_read_pos_, avail);

    segment_read_pos_ += avail;
    absolute_position_ += avail;
    return avail;
  }
  return 0;
}

bool DASHStream::seek(uint64_t const pos)
{
  // we seek only in the current segment
  if (pos >= absolute_position_ - segment_read_pos_)
  {
    segment_read_pos_ = static_cast<uint32_t>(pos - (absolute_position_ - segment_read_pos_));
    if (segment_read_pos_ > segment_buffer_.size())
    {
      segment_read_pos_ = static_cast<uint32_t>(segment_buffer_.size());
      return false;
    }
    absolute_position_ = pos;
    return true;
  }
  return false;
}

bool DASHStream::seek_time(double seek_seconds, double current_seconds, bool &needReset)
{
  if (!current_rep_)
    return false;

  uint32_t choosen_seg(~0);
  
  if (!current_adp_->segment_durations_.empty())
  {
    uint64_t sec_in_ts = static_cast<uint64_t>(seek_seconds * current_adp_->timescale_);
    choosen_seg = 0;
    while (choosen_seg < current_adp_->segment_durations_.size() && sec_in_ts > current_adp_->segment_durations_[choosen_seg])
      sec_in_ts -= current_adp_->segment_durations_[choosen_seg++];
  } 
  else if (current_rep_->flags_ & DASHTree::Representation::TIMELINE)
  {
    uint64_t sec_in_ts = static_cast<uint64_t>(seek_seconds * current_rep_->segtpl_.timescale);
    choosen_seg = (current_rep_->flags_ & DASHTree::Representation::INITIALIZATION)!=0 ? 1 : 0; //Skip initialization
    while (choosen_seg < current_rep_->segments_.size() && sec_in_ts > current_rep_->segments_[choosen_seg].range_begin_)
      ++choosen_seg;
  }
  else if (current_rep_->duration_ > 0 && current_rep_->timescale_ > 0)
  {
    uint64_t sec_in_ts = static_cast<uint64_t>(seek_seconds * current_rep_->timescale_);
    choosen_seg = static_cast<uint32_t>(sec_in_ts / current_rep_->duration_);
  }
  const DASHTree::Segment* old_seg(current_seg_);
  if ((current_seg_ = current_rep_->get_segment(choosen_seg, true)))
  {
    needReset = true;
    if (current_seg_ != old_seg)
      download_segment();
    else if (seek_seconds < current_seconds)
    {
      absolute_position_ -= segment_read_pos_;
      segment_read_pos_ = 0;
    }
    else
      needReset = false;
    return true;
  }
  else
    current_seg_ = old_seg;
  return false;
}

bool DASHStream::select_stream(bool force, bool justInit, unsigned int repId)
{
  const DASHTree::Representation *new_rep(0), *min_rep(0);

  if (force && absolute_position_ == 0) //already selected
    return true;

  if (!repId || repId > current_adp_->repesentations_.size())
  {
    unsigned int bestScore(~0);

    for (std::vector<DASHTree::Representation*>::const_iterator br(current_adp_->repesentations_.begin()), er(current_adp_->repesentations_.end()); br != er; ++br)
    {
      if ((*br)->bandwidth_ <= bandwidth_)
      {
        unsigned int score;
        if ((score = abs(static_cast<int>((*br)->width_ * (*br)->height_) - static_cast<int>(width_ * height_))
          + static_cast<unsigned int>(sqrt(bandwidth_ - (*br)->bandwidth_))) < bestScore)
        {
          bestScore = score;
          new_rep = (*br);
        }
        else if (!min_rep)
          min_rep = (*br);
      }
    }
  }
  else
    new_rep = current_adp_->repesentations_[repId -1];
  
  if (!new_rep)
    new_rep = min_rep;

  if (justInit)
  {
    current_rep_ = new_rep;
    return true;
  }

  if (!force && new_rep == current_rep_)
    return false;

  uint32_t segid(current_rep_ ? current_rep_->get_segment_pos(current_seg_) : 0);

  current_rep_ = new_rep;

  if (observer_)
    observer_->OnStreamChange(this, segid);

  /* If we have indexRangeExact SegmentBase, update SegmentList from SIDX */
  if (current_rep_->indexRangeMax_)
  {
    DASHTree::Representation *rep(const_cast<DASHTree::Representation *>(current_rep_));
    if (!parseIndexRange())
      return false;
    rep->indexRangeMin_ = rep->indexRangeMax_ = 0;
    stopped_ = false;
  }

  /* lets download the initialization */
  if ((current_seg_ = current_rep_->get_initialization()))
    return download_segment();

  stopped_ = true;
  return false;
}

void DASHStream::info(std::ostream &s)
{
  static const char* ts[4] = { "NoType", "Video", "Audio", "Text" };
  s << ts[type_] << " representation: " << current_rep_->url_.substr(current_rep_->url_.find_last_of('/') + 1) << " bandwidth: " << current_rep_->bandwidth_ << std::endl;
}

void DASHStream::clear()
{
  current_adp_ = 0;
  current_rep_ = 0;
}

DASHStream::~DASHStream()
{
  clear();
}
