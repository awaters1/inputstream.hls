/*
 * active_segment_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>

#include "active_segment_controller.h"

void ActiveSegmentController::download_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(download_mutex);
    std::cout << "Download waiting for conditional variable\n";
    download_cv.wait(lock, [this] {
      return (segments.size() < max_segment_data && has_next_download_segment())
          || quit_processing;
    });

    if (quit_processing) {
      std::cout << "Exiting download thread\n";
      return;
    }

    uint32_t download_index;
    hls::Segment segment;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      download_index = download_segment_index;
      segment = segments.at(download_index);
      segment_data[segment].state = SegmentState::DOWNLOADING;
    }

    std::cout << "Starting download of " << download_index << " at " << segment.get_url() << "\n";

    std::string contents = downloader->download(segment.get_url());
    {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        if (download_index == download_segment_index) {
          ++download_segment_index;
        } else {
          // Don't advance the index if it changed, it means we have changed
        	// where we are in the stream
        }
        segment_data[segment].state = SegmentState::DOWNLOADED;
    }
    lock.unlock();
    std::cout << "Finished download\n";
    {
      std::lock_guard<std::mutex> demux_lock(demux_mutex);
      last_downloaded_segment = segment;
      demux_cv.notify_one();
    }
  }
}

void ActiveSegmentController::demux_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(demux_mutex);
    std::cout << "Demux waiting for conditional variable\n";
    download_cv.wait(lock, [this] {
      return (has_next_demux_segment() || quit_processing);
    });

    if (quit_processing) {
      std::cout << "Exiting demux thread\n";
      return;
    }

    hls::Segment segment;
    SegmentData current_segment_data;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      segment = last_downloaded_segment;
      current_segment_data = segment_data[segment];
    }

    std::cout << "Starting decrypt and demux of " << segment.get_url() << "\n";
    // TODO: Implement decrypting and demuxing

    {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        segment_data[segment].state = SegmentState::DEMUXED;
    }

    lock.unlock();
    std::cout << "Finished decrypt and demux\n";
  }
}

std::future<std::unique_ptr<hls::ActiveSegment>> ActiveSegmentController::get_active_segment(hls::Segment segment) {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  SegmentData current_segment_data = segment_data[segment];
  switch(segment_data[segment].state) {
  case SegmentState::UNKNOWN:
    // TODO: Have signal to download
    break;
  case SegmentState::DOWNLOADING:
  case SegmentState::DOWNLOADED:
  case SegmentState::DEMUXING: {
    // TODO: Have to create a promise that returns a future
    std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
    segment_promises[segment] = std::move(promise);
    return promise.get_future();
  } case SegmentState::DEMUXED: {
    std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
    segment_data.erase(segment);
    promise.set_value(std::unique_ptr<hls::ActiveSegment>(current_segment_data.active_segment));
    return promise.get_future();
  }
  }
}

bool ActiveSegmentController::has_next_download_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return download_segment_index >= 0 && download_segment_index < segments.size();
}

bool ActiveSegmentController::has_next_demux_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return segment_data[last_downloaded_segment].state == SegmentState::DOWNLOADED;
}

void ActiveSegmentController::add_segment(hls::Segment segment) {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    segments.push_back(segment);
  }
  std::lock_guard<std::mutex> lock(download_mutex);
  download_cv.notify_one();
}

ActiveSegmentController::ActiveSegmentController(std::unique_ptr<Downloader> downloader) :
download_segment_index(0),
max_segment_data(10),
downloader(std::move(downloader)),
quit_processing(false) {
  download_thread = std::thread(&ActiveSegmentController::download_next_segment, this);
  demux_thread = std::thread(&ActiveSegmentController::demux_next_segment, this);
}

ActiveSegmentController::~ActiveSegmentController() {
  std::cout << "Deconstructing controller\n";
  quit_processing = true;
  {
    std::lock_guard<std::mutex> lock(download_mutex);
    download_cv.notify_all();
  }
  download_thread.join();
  demux_thread.join();
}

