/*
* DASHStream.h
*****************************************************************************
* Copyright (C) 2015, liberty_developer
*
* Email: liberty.developer@xmail.net
*
* This source code and its use and distribution, is subject to the terms
* and conditions of the applicable license agreement.
*****************************************************************************/

#pragma once

#include "DASHTree.h"
#include <string>

namespace dash
{
  class DASHStream;

  class DASHStreamObserver
  {
  public:
    virtual void OnStreamChange(DASHStream *stream, uint32_t segment) = 0;
  };

  class DASHStream
  {
  public:
    DASHStream(DASHTree &tree, DASHTree::StreamType type);
    ~DASHStream();
    void set_observer(DASHStreamObserver *observer){ observer_ = observer; };
    bool prepare_stream(const DASHTree::AdaptationSet *adp,
      const uint32_t width, const uint32_t height,
      uint32_t min_bandwidth, uint32_t max_bandwidth, unsigned int repId);
    bool start_stream(const uint32_t seg_offset, uint16_t width, uint16_t height);
    bool select_stream(bool force = false, bool justInit = false, unsigned int repId = 0);
    void stop(){ stopped_ = true; };
    void clear();
    void info(std::ostream &s);
    unsigned int getWidth() const { return width_; };
    unsigned int getHeight() const { return height_; };
    unsigned int getBandwidth() const { return bandwidth_; };

    unsigned int get_type()const{ return type_; };

    uint32_t read(void* buffer,
      uint32_t  bytesToRead);
    uint64_t tell(){ read(0, 0);  return absolute_position_; };
    bool seek(uint64_t const pos);
    bool seek_time(double seek_seconds, double current_seconds, bool &needReset);
    DASHTree::AdaptationSet const *getAdaptationSet() { return current_adp_; };
    DASHTree::Representation const *getRepresentation(){ return current_rep_; };
    double get_download_speed() const { return tree_.get_download_speed(); };
    void set_download_speed(double speed) { tree_.set_download_speed(speed); };
    size_t getSegmentPos() { return current_rep_->segments_.pos(current_seg_); };
    uint64_t GetPTSOffset() { return current_seg_ ? current_seg_->startPTS_ : 0; };
    // TODO: Temp move here
    bool download_segment();
    std::string segment_buffer_;
  protected:
    virtual bool download(const char* url, const char* rangeHeader){ return false; };
    virtual bool parseIndexRange() { return false; };
    bool write_data(const void *buffer, size_t buffer_size);
  private:


    DASHTree &tree_;
    DASHTree::StreamType type_;
    DASHStreamObserver *observer_;
    // Active configuration
    const DASHTree::Period *current_period_;
    const DASHTree::AdaptationSet *current_adp_;
    const DASHTree::Representation *current_rep_;
    const DASHTree::Segment *current_seg_;
    //We assume that a single segment can build complete frames

    std::size_t segment_read_pos_;
    uint64_t absolute_position_;

    uint16_t width_, height_;
    uint32_t bandwidth_;
    bool stopped_;
  };
};
