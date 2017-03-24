/*
 * active_segment_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <unordered_map>
#include <future>
#include <thread>
#include <vector>
#include <atomic>

#include "../hls/active_segment.h"
#include "downloader.h"

enum class SegmentState {
	UNKNOWN,
	DOWNLOADING,
	DOWNLOADED,
	DEMUXING,
	DEMUXED
};

struct SegmentData {
  SegmentState state = SegmentState::UNKNOWN;
  std::string content;
  hls::ActiveSegment* active_segment;
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
  void add_segment(hls::Segment segment);
  void add_segments(std::vector<hls::Segment> segments);
  std::future<std::unique_ptr<hls::ActiveSegment>> get_active_segment(hls::Segment segment);
private:
  bool has_next_demux_segment();
  bool has_next_download_segment();
  void download_next_segment();
  void demux_next_segment();
private:
  std::unique_ptr<Downloader> downloader;
  int max_segment_data;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  std::unordered_map<hls::Segment, SegmentData, SegmentHasher> segment_data;
  std::unordered_map<hls::Segment, std::promise<std::unique_ptr<hls::ActiveSegment>>, SegmentHasher> segment_promises;
  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  uint32_t download_segment_index;
  std::vector<hls::Segment> segments;

  std::vector<hls::Segment> last_downloaded_segments;

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
