/*
 * session.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <vector>

#include "HLS.h"
#include "../demux/demux.h"

namespace hls {
  class Stream {
    uint32_t stream_id;
  };

  class ActiveSegment {
  public:
    ActiveSegment(Segment segment): segment(segment) {}
  private:
    // Segment as defined in the playlist
    Segment segment;
    Demux *demux;
    std::string segment_buffer;
  };

  class Session {
  public:
    Session();
    ~Session();

    std::vector<Stream*> get_streams();
  private:
    ActiveSegment *active_segment;

    std::vector<Stream*> streams;
    uint32_t active_media_playlist;
    MasterPlaylist master_playlist;
  };
}
