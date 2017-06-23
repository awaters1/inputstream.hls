/*
 * playlist.h
 *
 *  Created on: Jun 21, 2017
 *      Author: anthony
 */

#pragma once

#include <list>
#include <vector>

#include "HLS.h"
#include "../globals.h"
#include "../downloader/downloader.h"

const int RELOAD_DELAY_MS = 500;

class SegmentDetails {
  uint32_t bandwidth;
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  bool discontinuity;
  uint32_t byte_length;
  uint32_t byte_offset;
};


class Segment {
public:
  Segment(hls::Segment segment, size_t num_variant_streams);
  void add_variant_segment(hls::Segment segment);
public:
  double duration;
  double time_in_playlist;
  uint32_t media_sequence;

  std::vector<SegmentDetails> details;
};

class VariantStream {
public:
  hls::MediaPlaylist playlist;
  // TODO: Init to segments.begin();
  // Points to the last added segment
  std::list<Segment>::const_iterator last_segment_itr;
};

class Stream {
public:
  Stream(Downloader *downloader, hls::MasterPlaylist master_playlist);
  ~Stream();
private:
  void reload_thread();
  void reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader);
private:
  Downloader *downloader;
  std::thread reload_thread;
  std::mutex data_mutex;
  std::condition_variable reload_cv;
  std::atomic_bool quit_processing;
private:
  bool live;
  bool all_loaded_once;
  std::vector<VariantStream> variants;
  std::list<Segment> segments;
  std::list<Segment>::const_iterator current_segment_itr;
};


