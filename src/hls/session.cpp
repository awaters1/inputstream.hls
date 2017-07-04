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


uint64_t hls::Session::get_current_time() {
  uint64_t current_time = current_pkt.current_time;
  if (current_time == 0) {
    return last_current_time;
  }
  last_current_time = current_time;
  return current_time;
}

DemuxContainer hls::Session::get_current_pkt() {
  if (!current_pkt.demux_packet) {
    read_next_pkt();
  }
  DemuxPacket *pkt = current_pkt.demux_packet;
  if (pkt && pkt->iStreamId != DMX_SPECIALID_STREAMCHANGE) {
    // When we have a discontinuity we have to modify the PTS values
    // of the incoming packets to match the existing stream
    // But we have to keep track of which times have a different start value
    // to take into account seeking
    bool discontinuity = current_pkt.discontinuity;
    if (discontinuity) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Detected a discontinuity at pts %f",
                            pkt->pts);
      m_startpts = DVD_NOPTS_VALUE;
      m_startdts = DVD_NOPTS_VALUE;
    }
    if (m_startpts == DVD_NOPTS_VALUE && pkt->pts != DVD_NOPTS_VALUE &&
        m_startdts == DVD_NOPTS_VALUE && pkt->dts != DVD_NOPTS_VALUE) {
      double desired_pts = current_pkt.time_in_playlist * DVD_TIME_BASE;
      double diff = pkt->pts - desired_pts;
      m_startpts = diff;
      m_startdts = diff;
    }

    if (pkt->pts != DVD_NOPTS_VALUE && m_startpts != DVD_NOPTS_VALUE) {
      pkt->pts = pkt->pts - m_startpts;
    }
    if (pkt->dts != DVD_NOPTS_VALUE && m_startdts != DVD_NOPTS_VALUE) {
      pkt->dts = pkt->dts - m_startdts;
    }
  }

  return current_pkt;
}

void hls::Session::read_next_pkt() {
  std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
   if (read_packet_buffer.empty()) {
     std::unique_lock<std::mutex> lock(demux_mutex);
     read_demux_cv.wait_for(lock, std::chrono::milliseconds(10), [&] {
       return quit_processing || !write_packet_buffer.empty();
     });
     read_packet_buffer.swap(write_packet_buffer);
     if (read_packet_buffer.size() > 0) {
       xbmc->Log(ADDON::LOG_NOTICE, LOGTAG "%s: Loaded %d packets", __FUNCTION__, read_packet_buffer.size());
     }
   }
   if (read_packet_buffer.empty()) {
     if (quit_processing) {
       xbmc->Log(ADDON::LOG_NOTICE, LOGTAG "%s: Quit read", __FUNCTION__);
       current_pkt = DemuxContainer();
       return;
     } else {
       // xbmc->Log(ADDON::LOG_NOTICE, LOGTAG "%s: Returning empty packet", __FUNCTION__);
       DemuxContainer container;
       container.demux_packet = ipsh->AllocateDemuxPacket(0);
       current_pkt = container;
       return;
     }
   }
   DemuxContainer packet = read_packet_buffer.front();
   read_packet_buffer.pop_front();
   std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

   if (duration > 10000) {
     // xbmc->Log(LOG_NOTICE, LOGTAG "%s: Read Duration %d, packets %d", __FUNCTION__, duration, readPacketBuffer.size());
   }
   if (read_packet_buffer.empty()) {
     read_start_time = 0;
     read_end_time = 0;
   } else {
     read_start_time = read_packet_buffer.front().current_time;
     read_end_time = read_packet_buffer.back().current_time;
   }
   current_pkt = packet;
}

void hls::Session::demux_process() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting demux thread");
  std::unique_ptr<Demux> demuxer;
  uint32_t last_variant_stream = -1;
  while(!quit_processing) {
    std::promise<std::unique_ptr<SegmentReader>> reader_promise;
    std::future<std::unique_ptr<SegmentReader>> reader_future = reader_promise.get_future();
    double seconds_in_buffers = 0;
    uint64_t write_start_time = 0;
    uint64_t write_end_time = 0;
    {
      std::lock_guard<std::mutex> lock(demux_mutex);
      if (!write_packet_buffer.empty()) {
        write_start_time = write_packet_buffer.front().current_time;
        write_end_time = write_packet_buffer.back().current_time;
      }
    }
    uint64_t total_time = 0;
    if (read_start_time == read_end_time) {
      total_time = write_end_time - write_start_time;
    } else if (write_end_time == write_start_time) {
      total_time = read_end_time - read_start_time;
    } else {
      total_time = write_end_time - read_start_time;
    }
    segment_storage.get_next_segment_reader(std::move(reader_promise), total_time);
    reader_future.wait_for(std::chrono::milliseconds(SEGMENT_TIMEOUT_DELAY));
    std::unique_ptr<SegmentReader> reader;
    try {
      reader = reader_future.get();
    } catch(std::exception &e) {
      quit_processing = true;
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exception getting segment %s", e.what());
      break;
    }
    // With the reader either create demux or use the existing demux
    // and demux the data coming in from the segment reader
    bool requires_new_demuxer = last_variant_stream != reader->get_variant_stream_index();
    last_variant_stream = reader->get_variant_stream_index();
    if (!demuxer || requires_new_demuxer) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Making a new demuxer");
      demuxer = std::make_unique<Demux>();
    }
    demuxer->set_segment_reader(std::move(reader));
    DemuxStatus status = DemuxStatus::FILLED_BUFFER;
    std::vector<DemuxContainer> demux_packets;

    while(status != DemuxStatus::SEGMENT_DONE && status != DemuxStatus::ERROR) {
      status = demuxer->Process(demux_packets);
      if (status == DemuxStatus::STREAM_SETUP_COMPLETE) {
        std::lock_guard<std::mutex> lock(demux_mutex);
        stream_ids.push_back(demuxer->GetStreamIds());
        streams.push_back(demuxer->GetStreams());
      }
      {
         std::unique_lock<std::mutex> lock(demux_mutex);
         write_packet_buffer.insert(write_packet_buffer.end(), demux_packets.begin(), demux_packets.end());
         xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s: Copied %d packets, total: %d", __FUNCTION__, demux_packets.size(),
             write_packet_buffer.size());
         demux_packets.clear();
      }
      if (quit_processing) {
        break;
      }
    }
    if (status != DemuxStatus::SEGMENT_DONE) {
        // TODO: Handle potential error
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Ending demux thread");
}

hls::MediaPlaylist hls::Session::download_playlist(std::string url) {
  FileMediaPlaylist media_playlist;
  media_playlist.open(url.c_str());
  return media_playlist;
}

INPUTSTREAM_IDS hls::Session::get_streams() {
  std::lock_guard<std::mutex> lock(demux_mutex);
  if (stream_ids.empty()) {
    INPUTSTREAM_IDS empty;
    empty.m_streamCount = 0;
    return empty;
  }
  INPUTSTREAM_IDS ids = stream_ids.front();
  stream_ids.pop_front();
  streams_read = 0;
  last_stream_count = ids.m_streamCount;
  return ids;
}

INPUTSTREAM_INFO hls::Session::get_stream(uint32_t stream_id) {
  std::lock_guard<std::mutex> lock(demux_mutex);
  INPUTSTREAM_INFO info;
  if (!streams.empty()) {
    INPUTSTREAM_INFO *streams_info = streams.front();
    for(size_t i = 0; i < last_stream_count; ++i) {
      if (streams_info[i].m_pID == stream_id) {
        info = streams_info[i];
      }
    }
    if (streams_read == last_stream_count) {
      streams.pop_front();
    }
  }
  return info;
}

bool hls::Session::seek_time(double time, bool backwards, double *startpts) {
//  if (active_stream) {
//    // time is in MSEC
//    double desired = time / 1000.0;
//
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s: bw:%d desired:%+6.3f", __FUNCTION__, backwards, desired);
//    if (active_stream->get_stream()->empty()) {
//      std::promise<void> promise;
//      std::future<void> future = promise.get_future();
//      active_stream->get_stream()->wait_for_playlist(std::move(promise));
//      future.wait();
//      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s: Playlist is ready", __FUNCTION__);
//    }
//
//    if (active_stream->get_stream()->empty()) {
//        return false;
//    }
//
//
//    hls::Segment seek_to = active_stream->get_stream()->find_segment_at_time(desired);
//    double new_time = seek_to.time_in_playlist;
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "seek to %+6.3f", new_time);
//
//    ActivePlaylist &active_playlist = active_stream->get_stream()->get_playlist();
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Using playlist %s", active_playlist.get_playlist_url().c_str());
//    active_stream = std::unique_ptr<StreamContainer>(
//        new StreamContainer(active_playlist, downloader.get(), seek_to.media_sequence));
//
//
//    if (current_pkt.demux_packet) {
//      ipsh->FreeDemuxPacket(current_pkt.demux_packet);
//      current_pkt.demux_packet = 0;
//    }
//
//    m_startdts = m_startpts = DVD_NOPTS_VALUE;
//
//    *startpts = (new_time * DVD_TIME_BASE);
//
//    // Cancel any stream switches
//    switch_demux = false;
//    if (future_stream) {
//      delete future_stream.release();
//    }
//    return true;
//  }
  return false;
}

hls::Session::Session(MasterPlaylist master_playlist, Downloader *downloader,
    int min_bandwidth, int max_bandwidth, bool manual_streams) :
    quit_processing(false),
    min_bandwidth(min_bandwidth),
    max_bandwidth(max_bandwidth),
    manual_streams(manual_streams),
    master_playlist(master_playlist),
    downloader(downloader),
    m_startpts(DVD_NOPTS_VALUE),
    m_startdts(DVD_NOPTS_VALUE),
    last_total_time(0),
    last_current_time(0),
    segment_storage(downloader, master_playlist){
  demux_thread = std::thread(&Session::demux_process, this);
}

uint64_t hls::Session::get_total_time() {
//  uint64_t current_total_time = active_stream->get_stream()->get_total_duration();
//  if (current_total_time == 0) {
//    return last_total_time;
//  }
//  last_total_time = current_total_time;
//  return current_total_time;
  return 0;
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
  // TODO: Implement
//  if (active_stream) {
//    active_stream->get_demux()->Flush();
//  }
//  if (future_stream) {
//    future_stream->get_demux()->Flush();
//  }
}

hls::Session::~Session() {
  if (current_pkt.demux_packet) {
    ipsh->FreeDemuxPacket(current_pkt.demux_packet);
  }
  {
    std::lock_guard<std::mutex> lock (demux_mutex);
    quit_processing = true;
  }
  demux_thread.join();
}
