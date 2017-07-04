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

#include "HLS.h"
#include "../downloader/downloader.h"
#include "../demuxer/demux.h"
#include "../segment_storage.h"

const int SEGMENT_TIMEOUT_DELAY = 60000;

namespace hls {

  class Session {
  public:
    Session(MasterPlaylist master_playlist, Downloader *downloader, int min_bandwidth, int max_bandwidth, bool manual_streams);
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
    INPUTSTREAM_IDS m_streamIds;
    INPUTSTREAM_INFO *m_streams;
  private:
    int min_bandwidth;
    int max_bandwidth;
    bool manual_streams;
  private:
    MasterPlaylist master_playlist;
    SegmentStorage segment_storage;

    DemuxContainer current_pkt;

    double m_startpts;          ///< start PTS for the program chain
    double m_startdts;          ///< start DTS for the program chain
    uint64_t last_total_time;
    uint64_t last_current_time;
  };
}
