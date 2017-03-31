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
    bool is_live() { return active_segment_controller->is_live(); };
    int read_stream(uint8_t *buf, size_t size);
  protected:
    virtual MediaPlaylist download_playlist(std::string url);
  private:
    void switch_streams();
    bool load_segments();

    uint32_t stall_counter;

    // Downloader has to be deleted last
    std::unique_ptr<Downloader> downloader;

    std::unique_ptr<ActiveSegment> active_segment;
    uint32_t active_segment_content_offset;

    std::unique_ptr<ActiveSegmentController> active_segment_controller;
    // For when we want to switch streams
    std::unique_ptr<ActiveSegmentController> future_segment_controller;


    MasterPlaylist master_playlist;
    uint32_t total_time;
  };
}
