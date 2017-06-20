/*
 * stream.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <algorithm>
#include "stream.h"
#define LOGTAG                  "[Stream] "

Stream::Stream(ActivePlaylist &playlist, uint32_t media_sequence) :
playlist(playlist),
media_sequence(media_sequence),
live(playlist.live),
download_itr(segments.end()),
set_promise(false) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting stream", __FUNCTION__);
}

Stream::~Stream() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Deconstruct stream", __FUNCTION__);
}

StreamContainer::StreamContainer(ActivePlaylist &playlist, Downloader *downloader, uint32_t media_sequence) :
stream(new Stream(playlist, media_sequence)),
segment_storage(new SegmentStorage(downloader, stream.get())),
demux(new Demux(segment_storage.get()))
{
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting stream container", __FUNCTION__);
}

void Stream::wait_for_playlist(std::promise<void> promise) {
  std::lock_guard<std::mutex> lock(data_mutex);
  if (segments.empty()) {
    set_promise = true;
    segment_promise = std::move(promise);
  } else {
    promise.set_value();
  }
}

bool Stream::is_live() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return live;
}

bool Stream::empty() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return segments.empty();
}

bool Stream::has_download_item() {
  std::lock_guard<std::mutex> lock(data_mutex);
  return download_itr != segments.end();
}

void Stream::reset_download_itr() {
  std::lock_guard<std::mutex> lock(data_mutex);
  download_itr = std::find_if(segments.begin(), segments.end(), [&](hls::Segment segment) -> bool {
      return segment.media_sequence == media_sequence;
  });
  if (download_itr == segments.end()) {
    download_itr = segments.begin();
  }
}

hls::Segment Stream::get_current_segment() {
  return *download_itr;
}

void Stream::go_to_next_segment() {
  ++download_itr;
}

uint64_t Stream::get_total_duration() {
  std::lock_guard<std::mutex> lock(data_mutex);
  double total_time = 0;
  for(auto it = segments.begin(); it != segments.end(); ++it) {
    total_time += it->duration;
  }
  return (uint64_t) total_time * 1000;
}

hls::Segment Stream::find_segment_at_time(double time_in_seconds) {
  std::lock_guard<std::mutex> lock(data_mutex);
  double running_total(0);
  for(auto it = segments.begin(); it != segments.end(); ++it) {
    if (running_total >= time_in_seconds) {
      if (it != segments.begin()) {
        return *(--it);
      } else {
        return *(segments.begin());
      }
    }
    running_total += it->duration;
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Unable to find segment at %f", time_in_seconds);
  return *(--segments.end());
}


void Stream::merge(hls::MediaPlaylist &other_playlist) {

}

