/*
 * SegmentReader.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "HLS.h"

class SegmentReader {
public:
  SegmentReader(hls::Segment segment, double time_in_playlist);
  virtual ~SegmentReader();
  void read(size_t pos, size_t len, uint8_t *buf, size_t min);
  hls::Segment get_segment();
  double get_time_in_playlist() {
    return time_in_playlist;
  }
  bool get_can_overwrite();
  bool is_finished();
  void write_data(std::string data);
  void end_data();
private:
  std::mutex data_mutex;
  double time_in_playlist;
  hls::Segment segment;
  std::string contents;
  bool can_overwrite;
  bool finished;
};
