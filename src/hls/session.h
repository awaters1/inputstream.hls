#pragma once
/*
 *      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <unordered_map>
#include <vector>
#include <future>
#include <thread>
#include <deque>
#include <atomic>
#include <cstdint>

#include "HLS.h"
#include "../downloader/downloader.h"
#include "../demuxer/demux.h"
#include "../segment_storage.h"

const int PACKET_TIMEOUT_MS = 10;
const int PACKET_STALLS_PER_FREEZE = 1000 / PACKET_TIMEOUT_MS;

namespace hls {

  class Session {
  public:
    Session(MasterPlaylist master_playlist, Downloader *downloader, int min_bandwidth, int max_bandwidth, bool manual_streams,
        std::unordered_map<StateAction, double> q_map);
    virtual ~Session();
    Session(const Session& other) = delete;
    Session & operator= (const Session & other) = delete;
    uint64_t get_total_time();

    INPUTSTREAM_IDS get_streams();
    INPUTSTREAM_INFO get_stream(uint32_t stream_id);

    DemuxContainer get_current_pkt();
    void read_next_pkt();
    uint64_t get_current_time();
    bool seek_time(double time, bool backwards, double *startpts);
    void demux_abort();
    void demux_flush();
  protected:
    virtual MediaPlaylist download_playlist(std::string url);
    // Downloader has to be deleted last
    std::unique_ptr<Downloader> downloader;
  private:
    void demux_process();
    std::thread demux_thread;
    std::mutex demux_mutex;
    std::condition_variable read_demux_cv;
    std::deque<DemuxContainer> read_packet_buffer;
    std::deque<DemuxContainer> write_packet_buffer;
    bool quit_processing;
    bool flush_demux;
    std::list<INPUTSTREAM_IDS> stream_ids;
    std::list<INPUTSTREAM_INFO*> streams;
    uint32_t last_stream_count;
    uint32_t streams_read;
  private:
    std::atomic<uint64_t> read_start_time;
    std::atomic<uint64_t> read_end_time;
    uint32_t packet_stalls;
    std::atomic<std::chrono::high_resolution_clock::time_point> last_freeze_time;
    std::atomic<uint32_t> total_freeze_duration_ms;
    std::atomic<uint32_t> number_of_freezes;
  private:
    int min_bandwidth;
    int max_bandwidth;
    bool manual_streams;
  private:
    MasterPlaylist master_playlist;
  protected:
    SegmentStorage segment_storage;
  private:

    DemuxContainer current_pkt;

    double m_startpts;          ///< start PTS for the program chain
    double m_startdts;          ///< start DTS for the program chain
  };
}
