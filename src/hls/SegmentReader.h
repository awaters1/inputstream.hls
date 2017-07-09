/*
 * SegmentReader.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <future>
#include "HLS.h"

const size_t READ_TIMEOUT_MS = 60000;

enum SegmentReaderStatus {
  FLUSHED,
  ENDED,
  SUCCESS
};

class SegmentReader {
public:
  SegmentReader(hls::Segment segment, double time_in_playlist, uint32_t variant_stream_index);
  virtual ~SegmentReader();
  hls::Segment get_segment();
  double get_time_in_playlist() {
    return time_in_playlist;
  }
  bool get_can_overwrite();
  bool is_finished();
  void write_data(std::string data);
  void end_data(bool flush);
  SegmentReaderStatus read(uint64_t pos, size_t &size, uint8_t * const destination, size_t min_read);
  uint32_t get_variant_stream_index() { return variant_stream_index; };
private:
  void read_impl(uint64_t pos, size_t &size, uint8_t * const destination);
private:
  std::condition_variable data_cv;
  std::mutex data_mutex;
  double time_in_playlist;
  hls::Segment segment;
  std::string contents;
  bool can_overwrite;
  bool finished;
  uint32_t variant_stream_index;
  SegmentReaderStatus status;
};
