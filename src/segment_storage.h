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
#include "downloader/downloader.h"
#include "hls/SegmentReader.h"

const int RELOAD_DELAY_MS = 1000;
const size_t MAX_SEGMENTS =  6;

struct DataHelper {
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  SegmentReader *segment_reader;
};

class DownloadSegment {
public:
  DownloadSegment(double time_in_playlist, hls::Segment segment, size_t index, size_t num_variant_streams);
  void add_variant_segment(hls::Segment segment, size_t index);
  double get_end_time() {
    return time_in_playlist + duration;
  };
public:
  double duration;
  double time_in_playlist;
  uint32_t media_sequence;

  std::vector<hls::Segment> details;
};

class VariantStream {
public:
  VariantStream(hls::MediaPlaylist playlist);
  hls::MediaPlaylist playlist;
  // Points to the last added segment
  std::list<DownloadSegment>::iterator last_segment_itr;
};

class SegmentStorage {
public:
  SegmentStorage(Downloader *downloader, hls::MasterPlaylist master_playlist);
  ~SegmentStorage();
  void get_next_segment_reader(std::promise<std::unique_ptr<SegmentReader>> promise);
public:
  // These three are all executed from another thread that stays the same
  SegmentReader * start_segment(hls::Segment segment, double time_in_playlist);
private:
  bool can_download_segment();
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
  bool has_download_item();
private:
  std::list<std::unique_ptr<SegmentReader>> segment_data;
  bool valid_promise;
  std::promise<std::unique_ptr<SegmentReader>> segment_reader_promise;
  bool quit_processing;
  bool no_more_data;
  Downloader *downloader;

  std::unordered_map<std::string, std::string> aes_uri_to_key;

  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;
  std::mutex data_lock;
  std::condition_variable data_cv;

private:
  void reload_playlist_thread();
  void reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader);
  bool live;
  bool all_loaded_once;
  std::vector<VariantStream> variants;
  std::list<DownloadSegment> segments;
  std::list<DownloadSegment>::const_iterator current_segment_itr;
  // Reload thread
  std::thread reload_thread;
  std::condition_variable reload_cv;
};
