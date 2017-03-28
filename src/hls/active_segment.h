/*
 * active_segment.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"

namespace hls {
  class ActiveSegment {
  public:
    ActiveSegment(Segment segment, std::string content):
      segment(segment),
      content(content)
  {}
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    uint32_t get_byte_length() { return segment.byte_length; };
    uint32_t get_byte_offset() { return segment.byte_offset; };

    std::string content;
  private:
    // Segment as defined in the playlist
    Segment segment;
  };
}

