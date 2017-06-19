/*
 * playlist_reloader.h
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */
#pragma once

#include "../globals.h'"
#include "HLS.h"

class PlaylistReloader {
private:
  void active_reload_thread();
  void background_reload_thread();
private:
  std::mutex data_mutex;
  std::condition_variable active_reload_cv;
  std::atomic_bool quit_processing;
  std::vector<ActivePlaylist> active_playlists;
};

class ActivePlaylist {
private:
  // TODO: protect with a mutex
  std::list<hls::Segment> segments;
  bool live;
private:
  hls::MediaPlaylist media_playlist;
  bool active;
};
