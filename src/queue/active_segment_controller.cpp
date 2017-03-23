/*
 * active_segment_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>

#include "active_segment_controller.h"

void ActiveSegmentController::download_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(download_mutex);
    download_cv.wait(lock, [this] {
      return (active_segments.size() < max_active_segments && has_next_download_segment())
          || quit_processing;
    });

    if (quit_processing) {
      std::cout << "Exiting download thread\n";
      return;
    }

    uint32_t download_index;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      download_index = download_segment_index;
    }

    std::cout << "Starting download of " << download_index << "\n";

    // TODO: Implement download
    {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        if (download_index == download_segment_index) {
          ++download_segment_index;
        } else {
          // TODO: Handle download_segment_index changing, we should still mark
          // the segment downloaded, but depends on if we cache the downloads or not
        }
    }

    lock.unlock();
    download_cv.notify_one();
    std::cout << "Finished download\n";
    download_promise.set_value("test");
  }
}

void ActiveSegmentController::background_job() {
  while(!quit_processing) {

  }
  std::cout << "Exiting background job\n";
}

std::future<std::string> ActiveSegmentController::get_download_segment(uint32_t download_segment_index) {
  // TODO: Need to somehow set the value of the promise when the download is done
  // or kick off the download based on the download_segment_index
  return download_promise.get_future();
}

bool ActiveSegmentController::has_next_download_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return download_segment_index >= 0 && download_segment_index < segments.size();
}

void ActiveSegmentController::add_segment(hls::Segment segment) {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    segments.push_back(segment);
  }
  std::lock_guard<std::mutex> lock(download_mutex);
  download_cv.notify_one();
}

ActiveSegmentController::ActiveSegmentController() :
segment_index(0),
download_segment_index(0),
max_active_segments(10),
quit_processing(false) {
  active_segments.reserve(max_active_segments);
  background_thread = std::thread(&ActiveSegmentController::background_job, this);
  download_thread = std::thread(&ActiveSegmentController::download_next_segment, this);
}

ActiveSegmentController::~ActiveSegmentController() {
  quit_processing = true;
  download_cv.notify_all();
  background_thread.join();
  download_thread.join();
}

