/*
 * playlist_reloader.cpp
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */

#include <algorithm>
#include "playlist_reloader.h"

#define LOGTAG "[PlaylistReloader] "

bool ActivePlaylist::is_live() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return live;
}

bool ActivePlaylist::is_empty() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return segments.empty();
}

std::string ActivePlaylist::get_playlist_url() {
  return media_playlist.get_url();
}

double ActivePlaylist::get_segment_target_duration() {
  return media_playlist.get_segment_target_duration();
}

bool ActivePlaylist::is_active() {
  return active;
}

void ActivePlaylist::merge(hls::MediaPlaylist other_playlist) {
  std::lock_guard<std::mutex> lock(data_mutex);
  live = other_playlist.live;
  auto other_segments = other_playlist.get_segments();
  if (segments.empty()) {
    segments.insert(segments.end(), other_segments.begin(), other_segments.end());
  } else {
    bool reset = false;
    if (download_itr == segments.end()) {
      reset = true;
      download_itr = --segments.end();
    }
    uint32_t added_segments(0);
    uint32_t last_added_sequence(0);
    uint32_t last_media_sequence(segments.back().media_sequence);
    double time_in_playlist_offset = segments.back().time_in_playlist + segments.back().duration;
    double time_in_playlist = 0;
    for(auto it = other_segments.begin(); it != other_segments.end(); ++it) {
       if (it->media_sequence > last_media_sequence) {
         it->time_in_playlist = time_in_playlist + time_in_playlist_offset;
         time_in_playlist += it->duration;
         segments.push_back(*it);
         last_added_sequence = it->media_sequence;
         if (added_segments < 10) {
           xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Added segment sequence %d", last_added_sequence);
         }
       }
    }
    if (reset) {
      ++download_itr;
    }
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Added segment sequence %d", last_added_sequence);
    if (download_itr != segments.end()) {
      while((download_itr->media_sequence - segments.front().media_sequence) >= SEGMENT_LIST_LIMIT && live) {
        hls::Segment front = segments.front();
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Erasing segment %d", front.media_sequence);
        segments.pop_front();
      }
    }
  }
  if (!segments.empty() && set_promise) {
    segment_promise.set_value();
    set_promise = false;
  }
}

PlaylistReloader::PlaylistReloader(Downloader *downloader) :
    downloader(downloader),
    quit_processing(false) {
  active_reload_thread = std::thread(&PlaylistReloader::reload_thread, this, true);
  background_reload_thread = std::thread(&PlaylistReloader::reload_thread, this, false);
}

PlaylistReloader::~PlaylistReloader() {
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    quit_processing = true;
  }
  reload_cv.notify_all();
}

void reload_playlist(ActivePlaylist &playlist, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloading playlist");
  if (playlist.is_live() || playlist.is_empty()) {
     std::string playlist_contents = downloader->download(playlist.get_playlist_url());
     if (!playlist_contents.empty()) {
       hls::MediaPlaylist new_media_playlist;
       new_media_playlist.set_url(playlist.get_playlist_url());
       new_media_playlist.load_contents(playlist_contents);
       playlist.merge(new_media_playlist);
     } else {
       xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Playlist %s is empty", playlist.get_playlist_url());
     }
  }
}

void PlaylistReloader::reload_thread(bool is_active) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting reload of threads");

  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(data_mutex);
    double target_duration = 4.0;
    for(auto &playlist : active_playlists) {
      target_duration = std::min(target_duration, playlist.get_segment_target_duration() / 2.0);
    }
    if (!is_active) {
        target_duration *= 4.0;
    }
    reload_cv.wait_for(lock, std::chrono::milliseconds((int) target_duration * 1000), [&] {
      return quit_processing;
    });

    if (quit_processing) {
      break;
    }
    for(auto &playlist : active_playlists) {
      if (playlist.is_active() == is_active) {
        reload_playlist(playlist, downloader);
        if (quit_processing) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(RELOAD_DELAY_MS));
      }
    }
    lock.unlock();
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
}
