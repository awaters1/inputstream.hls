/*
 * segment_data.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"

struct SegmentData {
  hls::Segment segment;
  std::string content;
  std::string processed_content;
};

