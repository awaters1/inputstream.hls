/*
 * session.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

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
  protected:
    virtual MediaPlaylist download_playlist(std::string url);
  private:
    void switch_streams();
    void process_demux();

    uint32_t stall_counter;


    MasterPlaylist master_playlist;
    MediaPlaylist &active_playlist;

    // Downloader has to be deleted last
    std::unique_ptr<Downloader> downloader;

    std::unique_ptr<Demux> active_demux;
    // For when we want to switch streams
    std::unique_ptr<Demux> future_demux;

    DemuxContainer current_pkt;

    // Demux Process thread
    std::mutex demux_mutex;
    std::condition_variable demux_cv;
    std::thread demux_thread;
    std::atomic_bool demux_flag;
    std::atomic_bool quit_processing;
  };
}
