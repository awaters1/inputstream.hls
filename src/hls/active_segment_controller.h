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

#include "HLS.h"
#include "../downloader/downloader.h"

static const int NUM_RELOAD_TRIES = 10;

struct DataHelper {
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  hls::Segment segment;
};

class SegmentStorage;

class ActiveSegmentController {
public:
  ActiveSegmentController(SegmentStorage *segment_storage, Downloader *downloader, hls::MediaPlaylist &media_playlist, uint32_t media_sequence);
  ~ActiveSegmentController();

  // @return true if we expect more data, false if we don't
  // expect to download more data
  bool trigger_download();
private:
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
  void reload_playlist();
private:
  Downloader *downloader;
  hls::MediaPlaylist &media_playlist;
  SegmentStorage *segment_storage;

  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  // Where the download is pointing
  int32_t download_segment_index;
  uint32_t media_sequence;


  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;

  // Reload playlist thread
  std::condition_variable reload_cv;
  std::thread reload_thread;
  std::atomic_bool reload_playlist_flag;

  std::atomic_bool quit_processing;
  std::atomic_bool download_segment;
};
