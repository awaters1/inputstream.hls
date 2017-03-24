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

hls::ActiveSegment::~ActiveSegment() {
  std::cout << "Deleting active segment for " << segment.media_sequence << "\n";
  for(std::vector<TSDemux::STREAM_PKT*>::iterator it = packets.begin(); it != packets.end(); ++it) {
      delete [] (*it)->data;
      delete *it;
  }
}

TSDemux::STREAM_PKT* hls::ActiveSegment::get_next_pkt() {
  if (packet_index < 0 || packet_index >= packets.size()) {
      return nullptr;
  }
  return packets[packet_index++];
}

uint64_t hls::Session::get_current_time() {
  if (current_pkt) {
    return active_segment->get_current_time();
  }
  return 0;
}

TSDemux::STREAM_PKT* hls::Session::get_current_pkt() {
  if (!current_pkt) {
    read_next_pkt();
  }
  return current_pkt;
}

void hls::Session::read_next_pkt() {
  if (active_segment) {
    current_pkt = active_segment->get_next_pkt();
    if (!current_pkt && load_segments()) {
      current_pkt = active_segment->get_next_pkt();
    } else if (!current_pkt) {
      if (active_segment) {
        delete active_segment;
        active_segment = 0;
      }
      if (previous_segment) {
        delete previous_segment;
        previous_segment = 0;
      }
    }
  } else {
    current_pkt = nullptr;
  }
}

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

void hls::Session::create_next_segment_future() {
  switch_streams();
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
}

bool hls::Session::load_segments() {
  timespec res, t1, t2;
  clock_getres(CLOCK_REALTIME, &res);

  clock_gettime(CLOCK_REALTIME, &t1);

  if (previous_segment) {
    delete previous_segment;
  }
  previous_segment = active_segment;
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
  if (next_segment_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      // TODO: We can also use this to lower the quality
    std::cout << "Next segment isn't ready, we will likely stall\n";
  }
  active_segment = next_segment_future.get();
  create_next_segment_future();

  clock_gettime(CLOCK_REALTIME, &t2);

  std::cout << "clock_gettime() : "
       << ((t2.tv_sec - t1.tv_sec)  + (float) (t2.tv_nsec - t1.tv_nsec) / 1000000000.0) * 1000.0
       << " ms" << std::endl;
  if (!active_segment) {
    return false;
  }
  return true;
}

std::vector<hls::Stream> hls::Session::get_streams() {
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_segment) {
    load_segments();
  }
  return active_segment->streams;
}

hls::Stream hls::Session::get_stream(uint32_t stream_id) {
  std::vector<hls::Stream> streams = get_streams();
  for(std::vector<hls::Stream>::iterator it = streams.begin(); it != streams.end(); ++it) {
    if (it->stream_id == stream_id) {
      return *it;
    }
  }
  return hls::Stream();
}

hls::Session::Session(MasterPlaylist master_playlist) :
    active_segment_sequence(-1),
    master_playlist(master_playlist),
    previous_segment(0),
    active_segment(0),
    total_time(0),
    start_pts(-1),
    current_pkt(0),
    download_speed(0),
    media_playlists(master_playlist.get_media_playlist()){
  active_playlist = media_playlists.at(0);
  std::vector<Segment> segments = active_playlist.get_segments();
  for(std::vector<hls::Segment>::iterator it = segments.begin(); it != segments.end(); ++it) {
    total_time += it->duration;
  }
  std::cout << "Total time is " << total_time << "\n";
}

hls::Session::~Session() {
  if (active_segment) {
    delete active_segment;
  }
  if (previous_segment) {
    delete previous_segment;
  }
  if (next_segment_future.valid()) {
      delete next_segment_future.get();
  }
}
