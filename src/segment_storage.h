#pragma once
/*
 *      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <mutex>
#include "hls/HLS.h"
#include "hls/segment_data.h"

const uint32_t MAX_SEGMENTS = 10;

class SegmentStorage {
public:
  SegmentStorage();
  bool has_data(uint64_t pos, size_t size);
  hls::Segment read(uint64_t pos, size_t &size, uint8_t * const destination);
public:
  // These three are all executed from another thread that stays the same
  bool start_segment(hls::Segment segment);
  void write_segment(hls::Segment segment, std::string data);
  void end_segment(hls::Segment segment);
private:
  size_t get_size();
private:
  uint64_t offset;
  uint32_t read_segment_data_index;
  uint32_t write_segment_data_index;
  std::mutex data_lock;
  std::vector<SegmentData> segment_data;
  std::vector<std::mutex> segment_locks;
};
