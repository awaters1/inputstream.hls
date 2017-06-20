/*
 * playlist_reloader.h
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */
#pragma once

#include "../globals.h"
#include "HLS.h"
#include "../downloader/downloader.h"

const int RELOAD_DELAY_MS = 500;

class ActivePlaylist {
public:
  bool is_live();
  bool is_empty();
  std::string get_playlist_url();
  void merge(hls::MediaPlaylist other_playlist);
  double get_segment_target_duration();
  bool is_active();
  uint32_t get_bandwidth() {
    return media_playlist.bandwidth;
  };
private:
  std::mutex data_mutex;
  std::list<hls::Segment> segments;
  bool live;
private:
  hls::MediaPlaylist media_playlist;
  bool active;
};

class PlaylistReloader {
public:
  // TODO: Have to get the media playlists into here
  PlaylistReloader(Downloader *downloader);
  ~PlaylistReloader();
private:
  void reload_thread(bool is_active);
  std::thread active_reload_thread;
  std::thread background_reload_thread;
private:
  Downloader *downloader;
  std::mutex data_mutex;
  std::condition_variable reload_cv;
  std::atomic_bool quit_processing;
  std::vector<ActivePlaylist> active_playlists;
};
