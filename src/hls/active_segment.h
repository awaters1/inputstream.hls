/*
 * active_segment.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"
#include "../demux/demux.h"

namespace hls {
  class Stream {
   public:
     uint32_t stream_id;
     std::string codec_name;
     uint32_t channels;
     uint32_t sample_rate;
     uint32_t bit_rate;
     uint32_t bits_per_sample;
     uint32_t fps_scale;
     uint32_t fps_rate;
   };

  class ActiveSegment {
  public:
    ActiveSegment(Segment segment, std::vector<TSDemux::STREAM_PKT*> packets, std::vector<Stream> streams):
      segment(segment),
      packets(packets),
      streams(streams),
      packet_index(0)
  {}
    ~ActiveSegment();
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    TSDemux::STREAM_PKT* get_next_pkt();
    // TODO: Doesn't return PTS of main segment
    int64_t get_current_time() { return packets.at(packet_index)->pts; };
    uint32_t get_byte_length() { return segment.byte_length; };
    uint32_t get_byte_offset() { return segment.byte_offset; };
    std::vector<Stream> streams;
  private:
    // Segment as defined in the playlist
    uint32_t packet_index;
    Segment segment;
    std::vector<TSDemux::STREAM_PKT*> packets;
  };
}

