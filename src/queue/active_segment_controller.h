/*
 * active_segment_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <vector>
#include <atomic>

#include "../hls/session.h"
#include "downloader.h"

enum class SegmentState {
	UNKNOWN,
	DOWNLOADING,
	DOWNLOADED,
	DEMUXED
};

struct SegmentData {
  SegmentState state = SegmentState::UNKNOWN;
  std::string content;
};

struct SegmentHasher {
	std::size_t operator()(hls::Segment segment) const {
		using std::size_t;
		using std::hash;
		using std::string;

		return (hash<string>()(segment.get_url()))
				^ (hash<uint32_t>()(segment.byte_length) >> 1)
				^ (hash<uint32_t>()(segment.byte_offset) << 1);
	}
};

class ActiveSegmentController {
public:
  ActiveSegmentController(std::unique_ptr<Downloader> downloader);
  ~ActiveSegmentController();
  hls::ActiveSegment get_next_segment();
  void add_segment(hls::Segment segment);
  std::future<std::string> get_download_segment(uint32_t download_segment_index);
private:
  bool has_next_demux_segment();
  bool has_next_download_segment();
  void download_next_segment();
  void demux_next_segment();
private:
  std::unique_ptr<Downloader> downloader;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  std::unordered_map<hls::Segment, SegmentData, SegmentHasher> segment_data;


  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  uint32_t download_segment_index;
  uint32_t segment_index;
  std::vector<hls::Segment> segments;
  int max_active_segments;
  // TODO: Change to ActiveSegment later
  std::vector<int> active_segments;

  hls::Segment last_downloaded_segment;

  // Download thread
  std::condition_variable download_cv;
  std::mutex download_mutex;
  std::thread download_thread;


  // Demux thread
  std::condition_variable demux_cv;
  std::mutex demux_mutex;
  std::thread demux_thread;

  std::atomic_bool quit_processing;
};
