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
#include "../downloader/downloader.h"
#include "active_segment_controller.h"

namespace hls {

  class Session {
  public:
    Session(MasterPlaylist master_playlist, Downloader *downloader);
    ~Session();
    Session(const Session& other) = delete;
    Session & operator= (const Session & other) = delete;
    uint32_t get_total_time() { return total_time; };
    bool is_live() { return active_playlist.live; };
    int read_stream(uint8_t *buf, size_t size);
  protected:
    virtual MediaPlaylist download_playlist(std::string url);

    double download_speed;
  private:
    void reload_media_playlist(MediaPlaylist &mediaPlaylist);
    void switch_streams();
    bool load_segments();

    std::unique_ptr<ActiveSegment> active_segment;
    ActiveSegmentController active_segment_controller;

    MediaPlaylist active_playlist;
    uint32_t active_segment_sequence;
    MasterPlaylist master_playlist;

    std::vector<MediaPlaylist> media_playlists;
    uint32_t total_time;
    uint64_t start_pts;
    uint32_t active_segment_content_offset;
  };
}
