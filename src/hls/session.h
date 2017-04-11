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

#include "HLS.h"
#include "../downloader/downloader.h"
#include "../demuxer/demux.h"

namespace hls {

  class Session {
  public:
    Session(MasterPlaylist master_playlist, Downloader *downloader);
    ~Session();
    Session(const Session& other) = delete;
    Session & operator= (const Session & other) = delete;
    uint64_t get_total_time();
    bool is_live() {
      return active_playlist.live;
    };

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
  private:
    void switch_streams(uint32_t media_sequence);

    uint32_t stall_counter;


    MasterPlaylist master_playlist;
    MediaPlaylist &active_playlist;

    // Downloader has to be deleted last
    std::unique_ptr<Downloader> downloader;

    std::unique_ptr<Demux> active_demux;
    // For when we want to switch streams
    std::unique_ptr<Demux> future_demux;
    bool switch_demux;

    DemuxContainer current_pkt;
  };
}
