/*
 * SegmentReader.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

class SegmentReader {
public:
  SegmentReader(double time_in_playlist);
  virtual ~SegmentReader();
  void read(size_t pos, size_t len, uint8_t *buf, size_t min);
  hls::Segment get_segment();
  double get_time_in_playlist() {
    return time_in_playlist;
  }
private:
  double time_in_playlist;
};
