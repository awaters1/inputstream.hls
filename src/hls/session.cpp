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


// TODO: Switch streams up and down based on
// 1. current bandwidth
// 2. If we able to keep our buffer full in active_segment_controller
// 3. If we stalled at all in get next segment
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
    active_playlist = next_active_playlist;
    active_segment_controller.set_media_playlist(active_playlist);
  }
}

bool hls::Session::load_segments() {
  timespec res, t1, t2;
  clock_getres(CLOCK_REALTIME, &res);
  clock_gettime(CLOCK_REALTIME, &t1);

  switch_streams();
  auto future = active_segment_controller.get_next_segment();
  if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      // We can also use this to lower the quality perhaps
    ++stall_counter;
    std::cout << "Next segment isn't ready, we will likely stall\n";
  } else {
    stall_counter = 0;
  }
  active_segment = future.get();
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
    stall_counter(0),
    media_playlists(master_playlist.get_media_playlist()){
  active_playlist = media_playlists.at(0);
  total_time = active_playlist.get_total_duration();
  active_segment_controller.set_media_playlist(active_playlist);
  std::cout << "Total time is " << total_time << "\n";

  load_segments();
}

hls::Session::~Session() {

}
