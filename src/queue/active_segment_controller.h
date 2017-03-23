/*
 * active_segment_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <vector>
#include <atomic>

#include "../hls/session.h"
#include "downloader.h"

class ActiveSegmentController {
public:
  ActiveSegmentController(std::unique_ptr<Downloader> downloader);
  ~ActiveSegmentController();
  hls::ActiveSegment get_next_segment();
  void add_segment(hls::Segment segment);
  std::future<std::string> get_download_segment(uint32_t download_segment_index);
private:
  bool has_next_download_segment();
  void background_job();
  void download_next_segment();

  std::unique_ptr<Downloader> downloader;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  uint32_t download_segment_index;
  uint32_t segment_index;
  std::vector<hls::Segment> segments;
  int max_active_segments;
  // TODO: Change to ActiveSegment later
  std::vector<int> active_segments;

  // Download thread
  std::promise<std::string> download_promise;
  std::condition_variable download_cv;
  std::mutex download_mutex;
  std::thread download_thread;

  // Controls background processing
  // to fire off downloads and demux
  std::thread background_thread;

  std::atomic_bool quit_processing;
};
