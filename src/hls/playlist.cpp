/*
 * playlist_reloader.cpp
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */

#include <algorithm>
#include "playlistr.h"

#define LOGTAG "[Playlist] "

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


// Each playlist should be loaded at least once
void reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloading playlist %d", variant_stream->playlist.bandwidth);
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

Stream::Stream() :
    live(true),
    all_loaded_once(false) {

}

void Stream::reload_thread(bool is_active) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting reload thread");
  std::vector<VariantStream>::iterator current_variant_stream = variants.begin();
  while(!quit_processing && (!all_loaded_once || live)) {
    std::unique_lock<std::mutex> lock(data_mutex);
    reload_cv.wait_for(lock, std::chrono::milliseconds(RELOAD_DELAY_MS), [&] {
      return quit_processing;
    });

    if (quit_processing) {
      break;
    }
    lock.unlock();
    reload_playlist(current_variant_stream, downloader);
    ++current_variant_stream;
    if (current_variant_stream == variants.end()) {
      all_loaded_once = true;
      current_variant_stream = variants.begin();
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
}
