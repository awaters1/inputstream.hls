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
    ActiveSegment(Segment segment):
      segment(segment),
      segment_buffer(""),
      packet_index(0)
  {}
    ~ActiveSegment();
    ActiveSegment(const ActiveSegment& other) = delete;
    ActiveSegment & operator= (const ActiveSegment & other) = delete;

    std::string get_url() { return segment.get_url(); }
    bool write_data(const void *buffer, size_t buffer_size);
    TSDemux::STREAM_PKT* get_next_pkt();
    void create_demuxer(std::string aes_key);
    void create_demuxer();
    int64_t get_current_time() { return demux->get_current_time(); };
    uint32_t get_byte_length() { return segment.byte_length; };
    uint32_t get_byte_offset() { return segment.byte_offset; };
    std::vector<Stream> streams;
  private:
    // Segment as defined in the playlist
    uint32_t packet_index;
    Segment segment;
    std::unique_ptr<Demux> demux;
    std::vector<TSDemux::STREAM_PKT*> packets;
    std::string segment_buffer;
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
    virtual bool download_segment(ActiveSegment *active_segment);
    virtual std::string download_aes_key(std::string aes_uri);
    virtual MediaPlaylist download_playlist(std::string url);

    double download_speed;
  private:
    void reload_media_playlist(MediaPlaylist &mediaPlaylist);
    void switch_streams();
    ActiveSegment* load_next_segment(Segment segment);
    bool load_segments();
    void create_next_segment_future();

    ActiveSegment *active_segment;
    ActiveSegment *previous_segment;
    std::future<ActiveSegment*> next_segment_future;
    TSDemux::STREAM_PKT* current_pkt;

    std::unordered_map<std::string, std::string> aes_uri_to_key;

    MediaPlaylist active_playlist;
    uint32_t active_segment_sequence;
    MasterPlaylist master_playlist;

    std::vector<MediaPlaylist> media_playlists;
    uint32_t total_time;
    uint64_t start_pts;
  };
}
