/*
 * active_segment_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <vector>

#include "../hls/session.h"

class ActiveSegmentQueue {
public:
  hls::ActiveSegment get_next_segment();
  void add_segments(std::vector<hls::Segment> segments_to_add);
  // TODO: Not sure what the signature for this should be just yet
  void set_active_segment();
private:
  uint32_t segment_index;
  std::vector<hls::Segment> segments;
};
