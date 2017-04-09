/*
 * segment_storage.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include "hls/HLS.h"
#include "hls/segment_data.h"

const uint32_t MAX_SEGMENTS = 10;

class SegmentStorage {
public:
  SegmentStorage();
  bool start_segment(hls::Segment segment);
  void write_segment(std::string data);
  void end_segment();
  bool has_data(uint64_t pos, size_t size);
  hls::Segment read(uint64_t pos, size_t size, uint8_t * const destination);
private:
  size_t get_size();
private:
  uint64_t offset;
  bool has_room;
  uint32_t read_segment_data_index;
  uint32_t write_segment_data_index;
  std::vector<SegmentData> segment_data;
};
