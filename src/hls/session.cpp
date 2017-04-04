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

uint64_t hls::Session::get_current_time() {
  if (active_segment_controller) {
    return active_segment_controller->get_current_time();
  }
  return 0;
}

DemuxContainer hls::Session::get_current_pkt() {
  if (!current_pkt.demux_packet) {
    read_next_pkt();
  }
  return current_pkt;
}

void hls::Session::read_next_pkt() {
  if (active_segment_controller) {
    if (future_segment_controller && future_segment_controller->is_ready()) {
      future_segment_controller->skip_to_pts(current_pkt.demux_packet->pts);
      std::cout << "Switched stream at PTS " << current_pkt.demux_packet->pts << "\n";
      active_segment_controller.swap(future_segment_controller);
      delete future_segment_controller.release();
    }
    current_pkt = active_segment_controller->get_next_segment();
    if (current_pkt.segment_changed) {
      switch_streams();
    }
  } else {
    current_pkt = DemuxContainer();
  }
}

hls::MediaPlaylist hls::Session::download_playlist(std::string url) {
  FileMediaPlaylist media_playlist;
  media_playlist.open(url.c_str());
  return media_playlist;
}


// Switch streams up and down based on
// 1. current bandwidth
// 2. If we able to keep our buffer full in active_segment_controller
// 3. If we stalled at all in get next segment
void hls::Session::switch_streams() {
  if (future_segment_controller) {
    // Skip stream switch if we are in the middle of one
    return;
  }
  // Bits per second
  uint32_t bandwith_of_current_stream = 0;
  double average_bandwidth = downloader->get_average_bandwidth();
  bool switch_up = false;
  if (active_segment_controller) {
    if (active_segment_controller->get_percentage_buffer_full() >= 0.10) {
      switch_up = true;
    }
    bandwith_of_current_stream = active_segment_controller->get_bandwidth_of_current_playlist();
    std::cout << "Switch Stream stalls: " << stall_counter << " buffer: " <<
        active_segment_controller->get_percentage_buffer_full() << " bandwidth: " <<
        average_bandwidth << "\n";
  }
  std::vector<MediaPlaylist> media_playlists = master_playlist.get_media_playlist();
  std::vector<MediaPlaylist>::iterator next_active_playlist = media_playlists.end();
  for(auto it = media_playlists.begin(); it != media_playlists.end(); ++it) {
    if (switch_up && it->bandwidth > bandwith_of_current_stream && it->bandwidth < average_bandwidth) {
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = it;
       std::cout << "(Up) Variant stream bandwidth: " << it->bandwidth << " url: " << it->get_url() << "\n";
    } else if (it->bandwidth > bandwith_of_current_stream && it->bandwidth < average_bandwidth) {
      // Switch down
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = it;
       std::cout << "(Down) Variant stream bandwidth: " << it->bandwidth  << " url: " << it->get_url() << "\n";
    }
  }

  if (active_segment_controller && next_active_playlist != media_playlists.end() &&
      *next_active_playlist != active_segment_controller->get_current_playlist()) {
    std::cout << "Switching to playlist " << next_active_playlist->get_url() <<
        " at segment " << active_segment_controller->get_current_segment().media_sequence << "\n";
    MediaPlaylist active_playlist = *next_active_playlist;
    total_time = active_playlist.get_total_duration();
    future_segment_controller = std::unique_ptr<ActiveSegmentController>(
        new ActiveSegmentController(downloader.get()));
    future_segment_controller->set_media_playlist(active_playlist,
        active_segment_controller->get_current_segment());
  } else if (!active_segment_controller) {
    active_segment_controller =
            std::unique_ptr<ActiveSegmentController>(new ActiveSegmentController(downloader.get()));
    if (next_active_playlist != media_playlists.end()) {
      active_segment_controller->set_media_playlist(*next_active_playlist);
    } else {
      active_segment_controller->set_media_playlist(*media_playlists.begin());
    }
  }
}

INPUTSTREAM_IDS hls::Session::get_streams() {
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_segment_controller) {
    return INPUTSTREAM_IDS();
  }
  return active_segment_controller->get_stream_ids();
}

INPUTSTREAM_INFO hls::Session::get_stream(uint32_t stream_id) {
  for(size_t i = 0; i < active_segment_controller->get_stream_ids().m_streamCount; ++i) {
    if (active_segment_controller->get_streams()[i].m_pID == stream_id) {
      return active_segment_controller->get_streams()[i];
    }
  }
  return INPUTSTREAM_INFO();
}

hls::Session::Session(MasterPlaylist master_playlist, Downloader *downloader) :
    master_playlist(master_playlist),
    total_time(0),
    active_segment_controller(nullptr),
    future_segment_controller(nullptr),
    downloader(downloader),
    active_segment_content_offset(0),
    stall_counter(0) {
  switch_streams();
}

hls::Session::~Session() {

}
