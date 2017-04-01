/*
 * active_segment.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"
#include "../demuxer/demux.h"
#include "../demux_container.h"

namespace hls {
  class ActiveSegment {
  public:
    ActiveSegment(Segment segment, std::unique_ptr<Demux> demux, std::string content):
      segment(segment),
      demux(std::move(demux)),
      content(content)
  {}
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    DemuxContainer* get_next_pkt();
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

