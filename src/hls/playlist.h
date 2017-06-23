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

class Stream {
public:
  Stream();
private:
  void reload_thread();
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

class VariantStream {
public:
  hls::MediaPlaylist playlist;
  std::list<Segment>::const_iterator last_segment_itr;
};

class Segment {
  double duration;
  double time_in_playlist;
  uint32_t media_sequence;

  std::vector<SegmentDetails> details;
};

class SegmentDetails {
  uint32_t bandwidth;
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  bool discontinuity;
  uint32_t byte_length;
  uint32_t byte_offset;
};
