/*
 * playlist.h
 *
 *  Created on: Jun 21, 2017
 *      Author: anthony
 */

#pragma once

#include <list>
#include <vector>

#include "HLS.h"

class Stream {
  size_t current_variant_index;
  std::vector<VariantStream> variants;
  std::list<Segment> segments;
  std::list<Segment>::const_iterator current_segment_itr;
};

class VariantStream {
  hls::MediaPlaylist playlist;
  std::list<Segment>::const_iterator last_segment_itr;
};

class Segment {
  double duration;
  double time_in_playlist;
  uint32_t media_sequence;

  std::vector<SegmentDetails> details;
};

class SegmentDetails {
  uint32_t bandwidth;
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  bool discontinuity;
  uint32_t byte_length;
  uint32_t byte_offset;
};
