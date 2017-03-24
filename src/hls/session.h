/*
 * session.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <future>
#include <thread>

#include "HLS.h"
#include "active_segment.h"
#include "../queue/downloader.h"
#include "../queue/active_segment_controller.h"

namespace hls {

  class Session {
  public:
    Session(MasterPlaylist master_playlist, Downloader *downloader);
    ~Session();
    Session(const Session& other) = delete;
    Session & operator= (const Session & other) = delete;

    std::vector<Stream> get_streams();
    Stream get_stream(uint32_t stream_id);

    TSDemux::STREAM_PKT* get_current_pkt();
    void read_next_pkt();
    uint64_t get_current_time();
    uint32_t get_total_time() { return total_time; };
    bool is_live() { return active_playlist.live; };
  protected:
    virtual MediaPlaylist download_playlist(std::string url);

    double download_speed;
  private:
    void reload_media_playlist(MediaPlaylist &mediaPlaylist);
    void switch_streams();
    bool load_segments();

    std::unique_ptr<ActiveSegment> active_segment;
    ActiveSegmentController active_segment_controller;
    TSDemux::STREAM_PKT* current_pkt;

    MediaPlaylist active_playlist;
    uint32_t active_segment_sequence;
    MasterPlaylist master_playlist;

    std::vector<MediaPlaylist> media_playlists;
    uint32_t total_time;
    uint64_t start_pts;
  };
}
