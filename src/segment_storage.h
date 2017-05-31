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

#include <unordered_map>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include "hls/HLS.h"
#include "hls/segment_data.h"
#include "downloader/downloader.h"

class Stream;

const size_t MAX_SEGMENTS = 1;
const size_t READ_TIMEOUT_MS = 10000;

struct DataHelper {
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  hls::Segment segment;
};


class SegmentStorage {
public:
  SegmentStorage(Downloader *downloader, Stream *stream);
  ~SegmentStorage();
  bool has_data(uint64_t pos, size_t size);
  hls::Segment read(uint64_t pos, size_t &size, uint8_t * const destination, size_t min_read);
public:
  // These three are all executed from another thread that stays the same
  bool start_segment(hls::Segment segment);
  void write_segment(hls::Segment segment, std::string data);
  void end_segment(hls::Segment segment);
private:
  hls::Segment read_impl(uint64_t pos, size_t &size, uint8_t * const destination);
  size_t get_size();
  bool can_download_segment();
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
private:
  uint64_t offset;
  uint32_t read_segment_data_index;
  uint32_t write_segment_data_index;
  std::vector<SegmentData> segment_data;
  std::vector<std::mutex> segment_locks;
  bool quit_processing;
  bool no_more_data;
  Downloader *downloader;
  Stream *stream;

  std::unordered_map<std::string, std::string> aes_uri_to_key;

  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;
  std::mutex data_lock;
  std::condition_variable data_cv;
};
