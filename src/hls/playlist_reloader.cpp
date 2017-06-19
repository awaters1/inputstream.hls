/*
 * playlist_reloader.cpp
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */

#include "playlist_reloader.h"

void reload_playlist(ActvePlaylist &playlist, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloading playlist");
  if (playlist.live || stream->empty()) {
     std::string playlist_contents = downloader->download(stream->get_playlist_url());
     if (playlist_contents.empty()) {
       std::this_thread::sleep_for(std::chrono::milliseconds(1000));
     }
     hls::MediaPlaylist new_media_playlist;
     new_media_playlist.set_url(stream->get_playlist_url());
     new_media_playlist.load_contents(playlist_contents);
     stream->merge(new_media_playlist);
  }
}


void PlaylistReloader::active_reload_thread() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting reload of threads");

  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(data_mutex);
    double target_duration = 4.0;
    for(auto &playist : active_playlists) {
      target_duration = std::min(target_duration, playlist.media_playlist.
                                 get_segment_target_duration() / 2.0);
    }
    active_reload_cv.wait_for(lock, std::chrono::milliseconds((int) target_duration * 1000), [&] {
      return quit_processing;
    });

    if (quit_processing) {
      break;
    }
    lock.unlock();
    reload_playlist(stream, downloader);
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
}

void PlaylistReloader::background_reload_thread() {
  // TODO: Reload the non-active playlist slower
}
