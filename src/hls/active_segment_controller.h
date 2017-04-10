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

class Demux;

class ActiveSegmentController {
public:
  ActiveSegmentController(Demux *demux, Downloader *downloader, hls::MediaPlaylist &media_playlist);
  ~ActiveSegmentController();

  // @return true if we expect more data, false if we don't
  // expect to download more data
  bool trigger_download();
  void set_start_segment(hls::Segment seek_to);
private:
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
  void reload_playlist();
private:
  Downloader *downloader;
  hls::MediaPlaylist &media_playlist;
  Demux *demux;

  // Maps a position in the byte buffer to a media sequence of a segment
  // the first segment is starts at byte 0
  uint64_t current_pos;
  std::unordered_map<uint64_t, uint64_t> pos_to_media_sequence;
  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  // Where the download is pointing
  int32_t download_segment_index;
  // Segment we started at, may be empty
  hls::Segment start_segment;


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
