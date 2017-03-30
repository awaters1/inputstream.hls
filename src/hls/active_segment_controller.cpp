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
      segment = media_playlist.get_segment(download_index);
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

    hls::ActiveSegment *active_segment = new hls::ActiveSegment(segment, content);
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
      std::cout << "Erased the segment data for " << segment.get_url() << "\n";
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }

    lock.unlock();
    std::cout << "Finished decrypt and demux\n";
  }
}

void ActiveSegmentController::reload_playlist() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(reload_mutex);
    std::cout << "Reload for conditional variable\n";
    std::chrono::microseconds timeout(static_cast<uint32_t>(media_playlist.get_segment_target_duration() * 0.5));
    reload_cv.wait_for(lock, timeout,[this] {
      return (reload_playlist_flag || quit_processing);
    });

    if (quit_processing) {
      std::cout << "Exiting reload thread\n";
      return;
    }

    reload_playlist_flag = false;

    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      // Not sure if we need to have this locked for the whole process
      std::cout << "Reloading playlist bandwidth: " << media_playlist.get_url() << "\n";
      if (media_playlist.live || media_playlist.get_number_of_segments() == 0) {
         std::string playlist_contents = downloader->download(media_playlist.get_url());
         hls::MediaPlaylist new_media_playlist;
         new_media_playlist.load_contents(playlist_contents);
         uint32_t added_segments = media_playlist.merge(new_media_playlist);
         std::cout << "Reloaded playlist with " << added_segments << " new segments\n";
      }
    }

    lock.unlock();
    std::cout << "Finished playlist reload\n";
    // Trigger a download
    {
      std::lock_guard<std::mutex> download_lock(download_mutex);
      download_cv.notify_one();
    }
    // Trigger get_next_segment
    {
      std::unique_lock<std::mutex> lock(next_segment_mutex);
      next_segment_cv.notify_all();
    }
  }
}

std::future<std::unique_ptr<hls::ActiveSegment>> ActiveSegmentController::get_next_segment() {
  hls::Segment segment;
  SegmentData current_segment_data;
  uint32_t segment_index;
  bool has_segment = false;
  bool live = false;
  int tries = 0;
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    std::cout << "Looking to get segment " << current_segment_index << "\n";
    segment_index = current_segment_index;
    has_segment = media_playlist.has_segment(segment_index);
    live = media_playlist.live;
  }
  while(!has_segment) {
    if (live && tries < NUM_RELOAD_TRIES) {
      // Have to stall until we have the segment and then check for it again
      // If we do not have segment then wait for a little bit before continuing to
      // get it again
      {
        std::lock_guard<std::mutex> lock(reload_mutex);
        reload_playlist_flag = true;
        reload_cv.notify_one();
      }
      {
        std::unique_lock<std::mutex> lock(next_segment_mutex);
        next_segment_cv.wait(lock);
        lock.unlock();
      }
      {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        has_segment = media_playlist.has_segment(segment_index);
      }
      if (!has_segment) {
        uint32_t reload_delay = 1000;
        std::cout << "Unable to load the next segment, " << reload_delay << " waiting to reload\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(reload_delay));
      }
    } else {
      // We are all done
      std::cout << "Playlist is finished we reached " << tries << " tries\n";
      std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
      promise.set_value(nullptr);
      return promise.get_future();
    }
  }
  bool trigger_download = false;
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    if (has_segment) {
      segment = media_playlist.get_segment(segment_index);
      ++current_segment_index;
      current_segment_data = segment_data[segment];
    }
    switch(segment_data[segment].state) {
      case SegmentState::UNKNOWN: {
        // TODO: May have to remove a segment from segment_data so this one gets processed
        std::cout << "Have to download segment " << segment.get_url() << "\n";
        trigger_download = true;
      } case SegmentState::DOWNLOADING:
      case SegmentState::DOWNLOADED:
      case SegmentState::DEMUXING: {
        std::cout << "Have to wait for segment " << segment.get_url() << "\n";
        std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
        auto future = promise.get_future();
        segment_promises[segment] = std::move(promise);
        return future;
      } case SegmentState::DEMUXED: {
        std::cout << "Segment is ready " << segment.get_url() << "\n";
        std::promise<std::unique_ptr<hls::ActiveSegment>> promise;
        segment_data.erase(segment);
        trigger_download = true;
        promise.set_value(std::unique_ptr<hls::ActiveSegment>(current_segment_data.active_segment));
        return promise.get_future();
      }
    }
  }
  if (trigger_download) {
    std::cout << "Get next segment triggering download\n";
    {
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }
  }
  return std::future<std::unique_ptr<hls::ActiveSegment>>();
}

bool ActiveSegmentController::has_next_download_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  bool has_segment = media_playlist.has_segment(download_segment_index);
  std::cout << "Checking if we have segment " << download_segment_index << ": " << has_segment << "\n";
  return has_segment;
}

bool ActiveSegmentController::has_next_demux_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return !last_downloaded_segments.empty();
}

void ActiveSegmentController::set_media_playlist(hls::MediaPlaylist media_playlist) {
  // Only update the playlist if they are different
  if (media_playlist != this->media_playlist) {
    hls::Segment current_segment = this->media_playlist.get_segment(current_segment_index);
    // TODO: Update current_segment_index to correspond to
    // where the current segment is in the new playlist
    // but we don't know where that is until the reload is done
    // TODO: Clear out segment data/promises
    // basically have to restart the pipeline
    this->media_playlist = media_playlist;
    {
      std::lock_guard<std::mutex> lock(reload_mutex);
      reload_playlist_flag = true;
      reload_cv.notify_one();
    }
  }
}

void ActiveSegmentController::set_current_segment(hls::Segment segment) {
  // TODO: This would update the current segment index
  // TODO: Would also have to flush segment data?
}

ActiveSegmentController::ActiveSegmentController(std::unique_ptr<Downloader> downloader) :
download_segment_index(0),
current_segment_index(0),
max_segment_data(10),
downloader(std::move(downloader)),
quit_processing(false) {
  download_thread = std::thread(&ActiveSegmentController::download_next_segment, this);
  demux_thread = std::thread(&ActiveSegmentController::demux_next_segment, this);
  reload_thread = std::thread(&ActiveSegmentController::reload_playlist, this);
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
  {
    std::lock_guard<std::mutex> lock(reload_mutex);
    reload_cv.notify_all();
  }
  download_thread.join();
  demux_thread.join();
  reload_thread.join();
}

