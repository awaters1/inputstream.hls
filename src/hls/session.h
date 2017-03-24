/*
 * session.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <future>
#include <thread>

#include "HLS.h"
#include "../demux/demux.h"
#include "../queue/active_segment_controller.h"

namespace hls {
  class Stream {
  public:
    uint32_t stream_id;
    std::string codec_name;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t bit_rate;
    uint32_t bits_per_sample;
  };

  class ActiveSegment {
  public:
    ActiveSegment(Segment segment, std::vector<TSDemux::STREAM_PKT*> packets, std::vector<Stream> streams):
      segment(segment),
      packets(packets),
      streams(streams),
      packet_index(0)
  {}
    ~ActiveSegment();
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    TSDemux::STREAM_PKT* get_next_pkt();
    // TODO: Doesn't return PTS of main segment
    int64_t get_current_time() { return packets.at(packet_index)->pts; };
    uint32_t get_byte_length() { return segment.byte_length; };
    uint32_t get_byte_offset() { return segment.byte_offset; };
    std::vector<Stream> streams;
  private:
    // Segment as defined in the playlist
    uint32_t packet_index;
    Segment segment;
    std::vector<TSDemux::STREAM_PKT*> packets;
  };

  class Session {
  public:
    Session(MasterPlaylist master_playlist);
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
