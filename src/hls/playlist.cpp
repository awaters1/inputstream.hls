/*
 * playlist_reloader.cpp
 *
 *  Created on: Jun 18, 2017
 *      Author: anthony
 */

#include <algorithm>
#include "playlistr.h"

#define LOGTAG "[Playlist] "

Segment::Segment(hls::Segment segment, size_t num_variant_streams) :
    details(num_variant_streams) {

}

void Stream::reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloading playlist %d", variant_stream->playlist.bandwidth);
  std::string url = variant_stream->playlist.get_url();
  std::string playlist_contents = downloader->download(url);
  if (!playlist_contents.empty()) {
   hls::MediaPlaylist new_media_playlist;
   new_media_playlist.set_url(url);
   new_media_playlist.load_contents(playlist_contents);
   {
     std::lock_guard<std::mutex> lock(data_mutex);
     std::vector<hls::Segment> playlist_segments = new_media_playlist.get_segments();
     for(auto& segment : playlist_segments) {
         if (segments.empty()) {
           segments.push_back(Segment(segment));
           variant_stream->last_segment_itr = segments.begin();
         } else {
           while (variant_stream->last_segment_itr != segments.end() &&
               segment.media_sequence <= variant_stream->last_segment_itr->media_sequence) {
             ++variant_stream->last_segment_itr;
           }
           if (variant_stream->last_segment_itr == segments.end()) {
             segments.push_back(Segment(segment));
             variant_stream->last_segment_itr = segments.begin();
           } else {
             variant_stream->last_segment_itr->add_variant_segment(segment);
           }
         }
     }
   }
  } else {
   xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Playlist %s is empty", url);
  }
}

Stream::Stream(Downloader *downloader, hls::MasterPlaylist master_playlist) :
    downloader(downloader),
    live(true),
    all_loaded_once(false) {

}

Stream::~Stream() {
  {
    std::lock_guard<std::mutex> lock(data_mutex);
    quit_processing = true;
  }
  reload_cv.notify_all();
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
      // TODO: Prune very old segments here
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
}
