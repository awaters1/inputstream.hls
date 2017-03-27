/*
 * active_segment_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <algorithm>

#include "active_segment_controller.h"
#include "../hls/decrypter.h"

void ActiveSegmentController::download_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(download_mutex);
    std::cout << "Download waiting for conditional variable\n";
    download_cv.wait(lock, [this] {
      return (segment_data.size() < max_segment_data && has_next_download_segment())
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

    std::string contents = downloader->download(segment.get_url(), segment.byte_offset, segment.byte_length);
    {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        if (download_index == download_segment_index) {
          ++download_segment_index;
        } else {
          // Don't advance the index if it changed, it means we have changed
        	// where we are in the stream
        }
        segment_data[segment].state = SegmentState::DOWNLOADED;
        segment_data[segment].content = contents;
    }
    lock.unlock();
    std::cout << "Finished download\n";
    {
      std::lock_guard<std::mutex> demux_lock(demux_mutex);
      last_downloaded_segments.push_back(segment);
      demux_cv.notify_one();
    }
  }
}

bool packet_sorter(TSDemux::STREAM_PKT *pkt1, TSDemux::STREAM_PKT *pkt2) {
  return pkt1->dts < pkt2->dts;
}

void ActiveSegmentController::demux_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(demux_mutex);
    std::cout << "Demux waiting for conditional variable\n";
    demux_cv.wait(lock, [this] {
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
      segment = last_downloaded_segments.front();
      last_downloaded_segments.erase(last_downloaded_segments.begin());
      current_segment_data = segment_data[segment];
    }
    // Trigger a download when we remove one
    {
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }

    std::cout << "Starting decrypt and demux of " << segment.get_url() << "\n";
    std::string content = current_segment_data.content;
    if (segment.encrypted) {
      std::cout << "Decrypting segment\n";
      auto aes_key_it = aes_uri_to_key.find(segment.aes_uri);
      std::string aes_key;
      if (aes_key_it == aes_uri_to_key.end()) {
          std::cout << "Getting AES Key from " << segment.aes_uri << "\n";
          aes_key = downloader->download(segment.aes_uri);
          aes_uri_to_key.insert({segment.aes_uri, aes_key});
      } else {
          aes_key = aes_key_it->second;
      }
      content = decrypt(aes_key, segment.aes_iv, content);
    }
    Demux *demux = new Demux(content);
    demux->Process();
    hls::ActiveSegment *active_segment = new hls::ActiveSegment(segment, std::unique_ptr<Demux>(demux));
    bool erased = false;
    {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        segment_data[segment].state = SegmentState::DEMUXED;
        segment_data[segment].active_segment = active_segment;
        auto it = segment_promises.find(segment);
        if (it != segment_promises.end()) {
          auto promise = std::move(it->second);
          promise.set_value(std::unique_ptr<hls::ActiveSegment>(active_segment));
          segment_promises.erase(segment);
          erased = true;
        }
    }

    if (erased) {
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }

    lock.unlock();
    std::cout << "Finished decrypt and demux\n";
  }
}

std::future<std::unique_ptr<hls::ActiveSegment>> ActiveSegmentController::get_active_segment(hls::Segment segment) {
  SegmentData current_segment_data;
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    current_segment_data = segment_data[segment];
  }
  switch(segment_data[segment].state) {
  case SegmentState::UNKNOWN: {
    // TODO: May have to remove a segment from segment_data so this one gets processed
    std::cout << "Have to download segment " << segment.get_url() << "\n";
    auto it = std::find(segments.begin(), segments.end(), segment);
    if (it == segments.end()) {
      std::cerr << "Unable to find segment in our list\n";
      return std::future<std::unique_ptr<hls::ActiveSegment>>();
    } else {
      uint32_t index = it - segments.begin();
      if (index != download_segment_index) {
        {
          std::lock_guard<std::mutex> lock(private_data_mutex);
          download_segment_index = index;
        }
        std::lock_guard<std::mutex> lock(download_mutex);
        download_cv.notify_one();
      }
    }
  } case SegmentState::DOWNLOADING:
  case SegmentState::DOWNLOADED:
  case SegmentState::DEMUXING: {
    std::cout << "Have to wait for segment " << segment.get_url() << "\n";
    std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
    auto future = promise.get_future();
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      segment_promises[segment] = std::move(promise);
    }
    return future;
  } case SegmentState::DEMUXED: {
    std::cout << "Segment is ready " << segment.get_url() << "\n";
    std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      segment_data.erase(segment);
    }
    {
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }
    promise.set_value(std::unique_ptr<hls::ActiveSegment>(current_segment_data.active_segment));
    return promise.get_future();
  }
  }
  return std::future<std::unique_ptr<hls::ActiveSegment>>();
}

bool ActiveSegmentController::has_next_download_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return download_segment_index >= 0 && download_segment_index < segments.size();
}

bool ActiveSegmentController::has_next_demux_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return !last_downloaded_segments.empty();
}

void ActiveSegmentController::add_segment(hls::Segment segment) {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    segments.push_back(segment);
  }
  std::lock_guard<std::mutex> lock(download_mutex);
  download_cv.notify_one();
}

void ActiveSegmentController::add_segments(std::vector<hls::Segment> segments) {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    for(auto it = segments.begin(); it != segments.end(); ++it) {
        this->segments.push_back(*it);
    }
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
  {
    std::lock_guard<std::mutex> lock(demux_mutex);
    demux_cv.notify_all();
  }
  download_thread.join();
  demux_thread.join();
}

