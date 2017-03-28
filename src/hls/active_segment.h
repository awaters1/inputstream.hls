/*
 * active_segment.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"
#include "../demuxer/demux.h"

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
    ActiveSegment(Segment segment, std::unique_ptr<Demux> demux, std::string content):
      segment(segment),
      demux(std::move(demux)),
      content(content)
  {}
    ~ActiveSegment();
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    DemuxPacket* get_next_pkt();
    // TODO: Doesn't return PTS of main segment
    int64_t get_current_time() { return demux->GetPlayingTime(); };
    uint32_t get_byte_length() { return segment.byte_length; };
    uint32_t get_byte_offset() { return segment.byte_offset; };
    INPUTSTREAM_IDS get_input_stream_ids() { return demux->GetStreamIds(); };
    INPUTSTREAM_INFO* get_input_stream_info() { return demux->GetStreams(); };

    std::string content;
  private:
    // Segment as defined in the playlist
    Segment segment;
    std::unique_ptr<Demux> demux;
  };
}

