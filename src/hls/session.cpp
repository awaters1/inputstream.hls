/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <climits>

#include "decrypter.h"

#include "session.h"

hls::MediaPlaylist hls::Session::download_playlist(std::string url) {
  FileMediaPlaylist media_playlist;
  media_playlist.open(url.c_str());
  return media_playlist;
}

void hls::Session::reload_media_playlist(MediaPlaylist &media_playlist) {
  std::cout << "Reloading playlist bandwidth: " << media_playlist.bandwidth << "\n";
  if (media_playlist.live || media_playlist.get_number_of_segments() == 0) {
     MediaPlaylist new_media_playlist = download_playlist(media_playlist.get_url());
     std::vector<Segment> new_segments = new_media_playlist.get_segments();
     uint32_t last_media_sequence;
     if (media_playlist.get_number_of_segments() > 0) {
       last_media_sequence = media_playlist.get_segments().back().media_sequence;
     } else {
       last_media_sequence = -1;
     }
     uint32_t added_segments = 0;
     uint32_t last_added_sequence = 0;
     for(std::vector<Segment>::iterator it = new_segments.begin(); it != new_segments.end(); ++it) {
         if (it->media_sequence > last_media_sequence || last_media_sequence == uint32_t(-1)) {
             media_playlist.add_segment(*it);
             ++added_segments;
             last_added_sequence = it->media_sequence;
         }
     }
     std::cout << "Reloaded playlist with " << added_segments << " new segments, last segment id: " << last_added_sequence << "\n";
  }
}

// TODO: This only checks for going to a high quality stream, not dropping to lower quality streams
void hls::Session::switch_streams() {
  uint32_t bandwith_of_current_stream = active_playlist.bandwidth;
  MediaPlaylist &next_active_playlist = active_playlist;
  bool switch_up = false;
  if (download_speed > bandwith_of_current_stream) {
    switch_up = true;
  } else {
    bandwith_of_current_stream = 0;
  }
  for(auto it = media_playlists.begin(); it != media_playlists.end(); ++it) {
    if (switch_up && it->bandwidth > bandwith_of_current_stream && it->bandwidth < download_speed &&
        *it != active_playlist) {
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = *it;
       std::cout << "(Up) Variant stream bandwidth: " << it->bandwidth << "\n";
    } else if (it->bandwidth > bandwith_of_current_stream && it->bandwidth < download_speed &&
        *it != active_playlist) {
      // Switch down
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = *it;
       std::cout << "(Down) Variant stream bandwidth: " << it->bandwidth << "\n";
    }
  }
  if (next_active_playlist != active_playlist) {
    std::cout << "Reloading variant playlist\n";
    reload_media_playlist(next_active_playlist);
    active_playlist = next_active_playlist;
  }
}

bool hls::Session::load_segments() {
  timespec res, t1, t2;
  clock_getres(CLOCK_REALTIME, &res);
  clock_gettime(CLOCK_REALTIME, &t1);
  /*
  uint32_t tries = 0;
  while(!next_segment_future.valid() && tries < 10) {
    std::cout << "Invalid next segment future, attempting to get synchronously\n";
    create_next_segment_future();
    if (!next_segment_future.valid() && active_playlist.live) {
      // Try to reload playlist
      float target_duration = active_playlist.get_segment_target_duration();
      uint32_t reload_delay = (uint32_t) target_duration * 0.5 * 1000;
      std::cout << "Unable to load the next segment, " << reload_delay << " waiting to reload\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(reload_delay));
    } else {
      break;
    }
    ++tries;
  }
  */
  // TODO: When to reload playlist
  /*
   *  switch_streams();
  if (!active_playlist.has_next_segment(active_segment_sequence)) {
    // Try to reload the playlist before bailing
    reload_media_playlist(active_playlist);
    if (!active_playlist.has_next_segment(active_segment_sequence)) {
      std::cerr << "Unable to get the next segment " << active_segment_sequence << std::endl;
      next_segment_future = std::future<ActiveSegment*>();
      return;
    }
  }
  Segment segment = active_playlist.get_next_segment(active_segment_sequence);
  active_segment_sequence = segment.media_sequence;
  std::cout << "Loading segment " << segment.media_sequence << "\n";
  // TODO: Update the segments in the controller
  // TODO: Request the segments from the controller
   */
  /// TODO: Doesn't work on the last segment
  hls::Segment segment = active_playlist.get_next_segment(active_segment_sequence);
  auto future = active_segment_controller.get_active_segment(segment);
  if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      // TODO: We can also use this to lower the quality
    std::cout << "Next segment isn't ready, we will likely stall\n";
  }
  active_segment = future.get();
  std::cout << "Got segment " << segment.get_url() << "\n";
  active_segment_sequence = segment.media_sequence;
  clock_gettime(CLOCK_REALTIME, &t2);

  std::cout << "clock_gettime() : "
       << ((t2.tv_sec - t1.tv_sec)  + (float) (t2.tv_nsec - t1.tv_nsec) / 1000000000.0) * 1000.0
       << " ms" << std::endl;
  if (!active_segment) {
    return false;
  }
  return true;
}

int hls::Session::read_stream(uint8_t *buf, size_t size) {
  size_t data_to_read = size;
  while(data_to_read > 0 && active_segment) {
    if (active_segment_content_offset >= active_segment->content.length()) {
      load_segments();
      if (!active_segment) {
        break;
      }
      active_segment_content_offset = 0;
    }
    std::string content = active_segment->content;
    size_t amount_already_read = size - data_to_read;
    size_t amount_to_read = data_to_read;
    if (amount_to_read + active_segment_content_offset >= content.length()) {
      amount_to_read = content.length() - active_segment_content_offset;
    }
    memcpy(buf + amount_already_read,
        content.c_str() + active_segment_content_offset, amount_to_read);
    active_segment_content_offset += amount_to_read;
    data_to_read -= amount_to_read;
  }
  return size - data_to_read;
}

hls::Session::Session(MasterPlaylist master_playlist, Downloader *downloader) :
    active_segment_sequence(-1),
    master_playlist(master_playlist),
    total_time(0),
    start_pts(-1),
    download_speed(0),
    active_segment_controller(std::unique_ptr<Downloader>(downloader)),
    active_segment_content_offset(0),
    media_playlists(master_playlist.get_media_playlist()){
  active_playlist = media_playlists.at(0);
  std::vector<Segment> segments = active_playlist.get_segments();
  for(std::vector<hls::Segment>::iterator it = segments.begin(); it != segments.end(); ++it) {
    total_time += it->duration;
  }
  active_segment_controller.add_segments(segments);
  std::cout << "Total time is " << total_time << "\n";

  load_segments();
}

hls::Session::~Session() {

}
