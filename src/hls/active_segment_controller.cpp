/*
 * active_segment_queue.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <algorithm>

#include "active_segment_controller.h"
#include "../hls/decrypter.h"
#include "../demuxer/demux.h"
#include "../globals.h"

#define LOGTAG                  "[ActiveSegmentController] "

void ActiveSegmentController::download_next_segment() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(private_data_mutex);
    download_cv.wait(lock, [this] {
      return (download_segment && media_playlist.has_segment(download_segment_index)) || quit_processing;
    });

    download_segment = false;

    if (quit_processing) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting download thread");
      return;
    }

    uint32_t download_index = download_segment_index;
    hls::Segment segment = media_playlist.get_segment(download_index);
    pos_to_media_sequence.insert({current_pos, segment.media_sequence});

    lock.unlock();

    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of %d at %s", segment.media_sequence, segment.get_url().c_str());

    DataHelper data_helper;
    data_helper.aes_iv = segment.aes_iv;
    data_helper.aes_uri = segment.aes_uri;
    data_helper.encrypted = segment.encrypted;

    uint64_t bytes_read = 0;

    downloader->download(segment.get_url(), segment.byte_offset, segment.byte_length,
        [this, &data_helper, &bytes_read](std::string data) -> bool {
          bytes_read += data.length();
          this->process_data(data_helper, data);
          if (quit_processing) {
            return false;
          }
          return true;
    });

    lock.lock();

    current_pos += bytes_read;
    pos_to_media_sequence.insert({current_pos -1, segment.media_sequence});

    if (download_index == download_segment_index) {
      ++download_segment_index;
    } else {
      // Don't advance the index if it changed, it means we have changed
      // where we are in the stream
    }
    lock.unlock();
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Finished download of %d", segment.media_sequence);
  }
}

void ActiveSegmentController::process_data(DataHelper &data_helper, std::string data) {
  if (data_helper.encrypted) {
    auto aes_key_it = aes_uri_to_key.find(data_helper.aes_uri);
    std::string aes_key;
    if (aes_key_it == aes_uri_to_key.end()) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Getting AES Key from %s", data_helper.aes_uri.c_str());
        aes_key = downloader->download(data_helper.aes_uri);
        aes_uri_to_key.insert({data_helper.aes_uri, aes_key});
    } else {
        aes_key = aes_key_it->second;
    }
    std::string next_iv = data.substr(data.length() - 16);
    data = decrypt(aes_key, data_helper.aes_iv, data);
    // Prepare the iv for the next segment
    data_helper.aes_iv = next_iv;
  }
  demux->PushData(data);
}

void ActiveSegmentController::reload_playlist() {
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(private_data_mutex);
    float segment_duration = media_playlist.get_segment_target_duration();
    if (media_playlist.valid || segment_duration <= 0) {
      segment_duration = 5;
    }
    std::chrono::seconds timeout(static_cast<uint32_t>(segment_duration));
    reload_cv.wait_for(lock, timeout,[this] {
      return ((reload_playlist_flag) || quit_processing);
    });

    if (quit_processing) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
      return;
    }
    reload_playlist_flag = false;

    if (!media_playlist.valid) {
      continue;
    }

    bool trigger_next_segment = false;
    // Not sure if we need to have this locked for the whole process
    if (media_playlist.live || media_playlist.get_number_of_segments() == 0) {
       std::string playlist_contents = downloader->download(media_playlist.get_url());
       hls::MediaPlaylist new_media_playlist;
       new_media_playlist.load_contents(playlist_contents);
       uint32_t added_segments = media_playlist.merge(new_media_playlist);
       xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloaded playlist with %d new segments bandwidth %d", added_segments, media_playlist.bandwidth);
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
         xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting with segment %d", segment_index);
         download_segment_index = segment_index;
       }
       trigger_next_segment = true;
    }

    lock.unlock();
    if (download_segment) {
      download_cv.notify_all();
    }
  }
}

bool ActiveSegmentController::trigger_download() {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    download_segment = true;
    if (!media_playlist.live && !media_playlist.has_segment(download_segment_index)) {
      return false;
    }
  }
  download_cv.notify_all();
  return true;
}

ActiveSegmentController::ActiveSegmentController(Demux *demux, Downloader *downloader, hls::MediaPlaylist &media_playlist) :
download_segment_index(0),
downloader(downloader),
media_playlist(media_playlist),
demux(demux),
current_pos(0),
quit_processing(false) {
  download_thread = std::thread(&ActiveSegmentController::download_next_segment, this);
  reload_thread = std::thread(&ActiveSegmentController::reload_playlist, this);
}

ActiveSegmentController::~ActiveSegmentController() {
  {
    std::lock_guard<std::mutex> lock(private_data_mutex);
    quit_processing = true;
  }
  download_cv.notify_all();
  reload_cv.notify_all();
  download_thread.join();
  reload_thread.join();
}

