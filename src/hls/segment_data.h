/*
 * segment_data.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"

struct SegmentData {
  SegmentData() : can_overwrite(true) {};
  hls::Segment segment;
  std::string contents;
  bool can_overwrite;
};

