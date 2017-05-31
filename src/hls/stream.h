#pragma once
/*
 * stream.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <list>
#include <memory>

#include "../globals.h"
#include "../downloader/downloader.h"
#include "HLS.h"
#include "../segment_storage.h"
#include "../demuxer/demux.h"

// Limit to 3000 segments in a playlist
// would be about 200.0 minutes
const int SEGMENT_LIST_LIMIT = 3000;

class Stream {
public:
  Stream(hls::MediaPlaylist &playlist, uint32_t media_sequence);
  Stream(const Stream& other) = delete;
  void operator=(const Stream& other) = delete;
  ~Stream();
  hls::MediaPlaylist &get_playlist() { return playlist; };
  std::string get_playlist_url() { return playlist.get_url(); };
  hls::MediaPlaylist &get_updated_playlist() {
    std::lock_guard<std::mutex> lock(data_mutex);
    playlist.live = live;
    playlist.set_segments(segments);
    return playlist;
  }
public:
  bool is_live();
  bool empty();
  void merge(hls::MediaPlaylist &other_playlist);
  bool has_download_item();
  void reset_download_itr();
  hls::Segment get_current_segment();
  void go_to_next_segment();
  uint64_t get_total_duration();
  hls::Segment find_segment_at_time(double time_in_seconds);
private:
  hls::MediaPlaylist &playlist;
  uint32_t media_sequence;
  std::list<hls::Segment> segments;
  bool live;
  std::list<hls::Segment>::const_iterator download_itr;
  std::mutex data_mutex;
};

class StreamContainer {
public:
  StreamContainer(hls::MediaPlaylist &playlist, Downloader *downloader, uint32_t media_sequence);
  void operator=(const StreamContainer& other) = delete;
  StreamContainer(const StreamContainer& other) = delete;
  Demux *get_demux() { return demux.get(); };
  Stream *get_stream() { return stream.get(); };
private:
  std::unique_ptr<Stream> stream;
  std::unique_ptr<SegmentStorage> segment_storage;
  std::unique_ptr<Demux> demux;
};
