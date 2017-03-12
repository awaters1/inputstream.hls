/*
 * session.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <vector>

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
  };

  class ActiveSegment {
  public:
    ActiveSegment(Segment segment): segment(segment), demux(0), segment_buffer("") {}
    ~ActiveSegment();

    std::vector<Stream> extract_streams();
    std::string get_url() { return segment.get_url(); }
    bool write_data(const void *buffer, size_t buffer_size);
  private:
    // Segment as defined in the playlist
    Segment segment;
    Demux *demux;
    std::string segment_buffer;
  };

  // TODO: Doesn't have assignment operator or copy constructor for active_segment
  class Session {
  public:
    Session(MasterPlaylist master_playlist);
    ~Session();

    std::vector<Stream> get_streams();
    Stream get_stream(uint32_t stream_id);
  protected:
    virtual bool download_segment(ActiveSegment *active_segment);
  private:
    ActiveSegment *active_segment;

    std::vector<Stream> streams;
    uint32_t active_media_playlist_index;
    uint32_t active_media_segment_index;
    MasterPlaylist master_playlist;
  };
}
