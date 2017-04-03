/*
 * active_segment_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <algorithm>

#include "active_segment_controller.h"
#include "../hls/decrypter.h"

void ActiveSegmentController::download_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(download_mutex);
    std::cout << "Download waiting for conditional variable\n";
    download_cv.wait(lock, [this] {
      return (has_next_download_segment()) || quit_processing;
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
      SegmentData segment_data;
      segment_data.segment = segment;
      segment_data.content = contents;
      last_downloaded_segments.push_back(segment_data);
    }
    lock.unlock();
    {
      std::lock_guard<std::mutex> demux_lock(demux_mutex);
      demux_cv.notify_one();
    }
    std::cout << "Finished download of " << segment.media_sequence << "\n";
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

    // Demux everything that is downloaded
    while(has_next_demux_segment()) {
      hls::Segment segment;
      SegmentData current_segment_data;
      {
        std::lock_guard<std::mutex> lock(private_data_mutex);
        current_segment_data = last_downloaded_segments.front();
        last_downloaded_segments.erase(last_downloaded_segments.begin());
      }

      std::cout << "Starting decrypt and demux of " << segment.media_sequence << " url: " << segment.get_url() << "\n";
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
      current_segment_data.processed_content = content;

      {
          std::lock_guard<std::mutex> lock(private_data_mutex);
          demux->PushData(current_segment_data);
      }
      std::cout << "Finished decrypt and demux of " << segment.media_sequence << "\n";
    }
    lock.unlock();
    {
      std::lock_guard<std::mutex> lock(download_mutex);
      download_cv.notify_one();
    }
  }
}

void ActiveSegmentController::reload_playlist() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(reload_mutex);
    float segment_duration;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      if (media_playlist.valid) {
        segment_duration = media_playlist.get_segment_target_duration();
      } else {
        segment_duration = 20;
      }
    }
    std::chrono::seconds timeout(static_cast<uint32_t>(segment_duration * 0.5));
    reload_cv.wait_for(lock, timeout,[this] {
      return ((reload_playlist_flag) || quit_processing);
    });

    if (quit_processing) {
      std::cout << "Exiting reload thread\n";
      return;
    }
    reload_playlist_flag = false;

    if (!media_playlist.valid) {
      continue;
    }

    bool trigger_next_segment = false;
    {
      std::lock_guard<std::mutex> lock(private_data_mutex);
      // Not sure if we need to have this locked for the whole process
      if (media_playlist.live || media_playlist.get_number_of_segments() == 0) {
         std::string playlist_contents = downloader->download(media_playlist.get_url());
         hls::MediaPlaylist new_media_playlist;
         new_media_playlist.load_contents(playlist_contents);
         uint32_t added_segments = media_playlist.merge(new_media_playlist);
         std::cout << "Reloaded playlist with " << added_segments << " new segments bandwidth: " << media_playlist.bandwidth << "\n";
      }
      if (media_playlist.get_number_of_segments() > 0) {
         if (download_segment_index == -1) {
           int32_t segment_index;
           if (start_segment.valid) {
             // Find this segment in our segments
             segment_index = media_playlist.get_segment_index(start_segment);
           } else {
             // Just start at the beginning
             segment_index = 0;
           }
           std::cout << "Starting with segment " << segment_index << "\n";
           download_segment_index = segment_index;
         }
         trigger_next_segment = true;
      }
    }

    lock.unlock();
    // std::cout << "Finished playlist reload\n";
    // Trigger a download
    {
      std::lock_guard<std::mutex> download_lock(download_mutex);
      download_cv.notify_one();
    }
  }
}

DemuxContainer ActiveSegmentController::get_next_segment() {
  if (demux->get_percentage_buffer_full() < 1) {
    std::lock_guard<std::mutex> lock(download_mutex);
    download_cv.notify_all();
  }
  return demux->Read();
}

bool ActiveSegmentController::has_next_download_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  bool has_segment = download_segment_index >= 0 && media_playlist.has_segment(download_segment_index);
  std::cout << "Checking if we have segment " << download_segment_index << ": "
      << has_segment << " buffer: " << demux->get_percentage_buffer_full() << "\n";
  return has_segment && demux->get_percentage_buffer_full() < 1;
}

bool ActiveSegmentController::has_next_demux_segment() {
  std::lock_guard<std::mutex> lock(private_data_mutex);
  return !last_downloaded_segments.empty();
}

void ActiveSegmentController::set_media_playlist(hls::MediaPlaylist new_media_playlist) {
  set_media_playlist(new_media_playlist, hls::Segment());
}

void ActiveSegmentController::set_media_playlist(hls::MediaPlaylist new_media_playlist,
    hls::Segment active_segment) {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    start_segment = active_segment;
    media_playlist = new_media_playlist;
  }
  {
    std::lock_guard<std::mutex> lock(reload_mutex);
    reload_playlist_flag = true;
    reload_cv.notify_one();
  }
}

// Used for seeking in a stream
void ActiveSegmentController::set_current_segment(hls::Segment segment) {
  // TODO: This would update the current segment index
  // Would also have to flush segment data?
}

hls::Segment ActiveSegmentController::get_current_segment() {
  return demux->get_current_segment();
}

ActiveSegmentController::ActiveSegmentController(Downloader *downloader) :
download_segment_index(-1),
max_segment_data(2),
downloader(downloader),
demux(std::unique_ptr<Demux>(new Demux())),
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

