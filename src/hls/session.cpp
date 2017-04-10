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
#include "../globals.h"

#define LOGTAG                  "[SESSION] "

const double BUFFER_LOWER_BOUND = 0.5;

uint64_t hls::Session::get_current_time() {
  return current_pkt.current_time;
}

DemuxContainer hls::Session::get_current_pkt() {
  if (!current_pkt.demux_packet) {
    read_next_pkt();
  }
  return current_pkt;
}

void hls::Session::read_next_pkt() {
  if (active_demux) {
    if (future_demux) {
      // future_segment_controller->skip_to_pts(current_pkt.demux_packet->pts);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Switched stream at PTS %d", current_pkt.demux_packet->pts);
      active_demux.swap(future_demux);
      delete future_demux.release();
    }
    if (!active_demux->IsStreamDone() && active_demux->get_percentage_packet_buffer_full() < BUFFER_LOWER_BOUND) {
      std::lock_guard<std::mutex> lock(demux_mutex);
      demux_flag = true;
    }
    demux_cv.notify_all();
    current_pkt = active_demux->Read();
    if (current_pkt.segment_changed) {
      switch_streams();
    }
  } else {
    xbmc->Log(ADDON::LOG_ERROR, LOGTAG "No active demux, unable to get data");
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
  if (future_demux) {
    // Skip stream switch if we are in the middle of one
    return;
  }
  // Bits per second
  uint32_t bandwith_of_current_stream = 0;
  double average_bandwidth = downloader->get_average_bandwidth();
  bool switch_up = false;
  if (active_demux) {
    if (active_demux->get_percentage_packet_buffer_full() >= 0.10) {
      switch_up = true;
    }
    bandwith_of_current_stream = active_playlist.bandwidth;
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Switch Stream stalls: %d buffer: %f bandwidth: %d",
        stall_counter, active_demux->get_percentage_packet_buffer_full(), average_bandwidth);
  }
  std::vector<MediaPlaylist> &media_playlists = master_playlist.get_media_playlists();
  auto next_active_playlist = media_playlists.end();
  for(auto it = media_playlists.begin(); it != media_playlists.end(); ++it) {
    if (switch_up && it->bandwidth > bandwith_of_current_stream && it->bandwidth < average_bandwidth) {
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = it;
       xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "(Up) Variant stream bandwidth: %d url: %s", it->bandwidth, it->get_url().c_str());
    } else if (it->bandwidth > bandwith_of_current_stream && it->bandwidth < average_bandwidth) {
      // Switch down
       bandwith_of_current_stream = it->bandwidth;
       next_active_playlist = it;
       xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "(Down) Variant stream bandwidth: %d url: %s", it->bandwidth, it->get_url().c_str());
    }
  }

  if (active_demux && next_active_playlist != media_playlists.end() &&
      *next_active_playlist != active_playlist) {
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Switching to playlist %s", next_active_playlist->get_url().c_str());
    active_playlist = *next_active_playlist;
    future_demux = std::unique_ptr<Demux>(
        new Demux(downloader.get(), active_playlist));
    // TODO: Need to seek the future_demux as well
  } else if (!active_demux) {
    if (next_active_playlist != media_playlists.end()) {
      active_playlist = *next_active_playlist;
    } else {
      active_playlist = *media_playlists.begin();
    }
    active_demux =
            std::unique_ptr<Demux>(new Demux(downloader.get(), active_playlist));
    {
      std::lock_guard<std::mutex> lock(demux_mutex);
      demux_flag = true;
    }
    demux_cv.notify_all();
  }
}

INPUTSTREAM_IDS hls::Session::get_streams() {
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_demux) {
    INPUTSTREAM_IDS ids = INPUTSTREAM_IDS();
    ids.m_streamCount = 0;
    return ids;
  }
  return active_demux->GetStreamIds();
}

INPUTSTREAM_INFO hls::Session::get_stream(uint32_t stream_id) {
  for(size_t i = 0; i < get_streams().m_streamCount; ++i) {
    if (active_demux->GetStreams()[i].m_pID == stream_id) {
      return active_demux->GetStreams()[i];
    }
  }
  return INPUTSTREAM_INFO();
}

bool hls::Session::seek_time(double time, bool backwards, double *startpts) {
  if (active_demux) {
    bool seeked =  active_demux->SeekTime(time, backwards, startpts);
    if (seeked) {
      if (current_pkt.demux_packet) {
        ipsh->FreeDemuxPacket(current_pkt.demux_packet);
      }
      current_pkt = DemuxContainer();
      demux_flag = true;
      demux_cv.notify_all();
    }
    return seeked;
  }
  return false;
}

hls::Session::Session(MasterPlaylist master_playlist, Downloader *downloader) :
    master_playlist(master_playlist),
    active_demux(nullptr),
    future_demux(nullptr),
    downloader(downloader),
    active_playlist(this->master_playlist.get_media_playlist(0)),
    quit_processing(false),
    demux_flag(false),
    stall_counter(0) {
  switch_streams();
  demux_thread = std::thread(&hls::Session::process_demux, this);
}

void hls::Session::process_demux() {
  while(!quit_processing) {
     std::unique_lock<std::mutex> lock(demux_mutex);
     demux_cv.wait(lock, [this] {
       return quit_processing || demux_flag;
     });
     demux_flag = false;
     lock.unlock();

     if (quit_processing) {
       xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting demux thread");
       return;
     }

     while(active_demux && !active_demux->IsStreamDone() && active_demux->get_percentage_packet_buffer_full() < BUFFER_LOWER_BOUND) {
       active_demux->Process();
     }
  }
}

uint64_t hls::Session::get_total_time() {
  return active_playlist.get_total_duration();
}

void hls::Session::demux_abort() {
  // TODO: Causes issues so disable for now
  // in order to properly implement we need a lock on them
  // in case other threads are using them
//  if (active_demux) {
//    delete active_demux.get();
//  }
//  if (future_demux) {
//    delete future_demux.get();
//  }
}

void hls::Session::demux_flush() {
  if (active_demux) {
    active_demux->Flush();
  }
  if (future_demux) {
    future_demux->Flush();
  }
}

hls::Session::~Session() {
  {
    std::lock_guard<std::mutex> lock(demux_mutex);
    quit_processing = true;
  }
  demux_cv.notify_all();
  demux_thread.join();
  if (current_pkt.demux_packet) {
    ipsh->FreeDemuxPacket(current_pkt.demux_packet);
  }
}
