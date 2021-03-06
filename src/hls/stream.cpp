/*
 * stream.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <algorithm>
#include "stream.h"
#define LOGTAG                  "[Stream] "

Stream::Stream(hls::MediaPlaylist &playlist, uint32_t media_sequence) :
playlist(playlist),
media_sequence(media_sequence),
segments(playlist.get_segments().begin(), playlist.get_segments().end()),
live(playlist.live),
download_itr(segments.end()),
set_promise(false) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting stream", __FUNCTION__);
}

Stream::~Stream() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Deconstruct stream", __FUNCTION__);
}

StreamContainer::StreamContainer(hls::MediaPlaylist &playlist, Downloader *downloader, uint32_t media_sequence) :
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

