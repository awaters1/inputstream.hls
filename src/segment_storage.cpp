/*
 * segment_storage.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>

#include "globals.h"
#include "segment_storage.h"
#include "downloader/file_downloader.h"
#include "hls/decrypter.h"

#define LOGTAG                  "[SegmentStorage] "

SegmentStorage::SegmentStorage(Downloader *downloader, hls::MasterPlaylist master_playlist) :
offset(0),
read_segment_data_index(0),
write_segment_data_index(0),
segment_data(MAX_SEGMENTS),
downloader(downloader),
quit_processing(false),
no_more_data(false),
live(true),
all_loaded_once(false) {
  // TODO: Decompose the master_playlist into the variant streams
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting segment storage", __FUNCTION__);
  download_thread = std::thread(&SegmentStorage::download_next_segment, this);
  reload_thread = std::thread(&SegmentStorage::reload_playlist_thread, this);
  download_cv.notify_all();
}

bool SegmentStorage::can_download_segment() {
  // Called from a locked method
  std::unique_ptr<SegmentReader> &current_segment_reader = segment_data.at(write_segment_data_index);
  return current_segment_reader && current_segment_reader->get_can_overwrite();
}

bool SegmentStorage::start_segment(hls::Segment segment, double time_in_playlist) {
  std::lock_guard<std::mutex> lock(data_lock);
  std::unique_ptr<SegmentReader> &current_segment_reader = segment_data.at(write_segment_data_index);
  if (current_segment_reader && !current_segment_reader->get_can_overwrite()) {
      return false;
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Start segment %d at %d", __FUNCTION__,
      segment.media_sequence);
  segment_data[write_segment_data_index] =
      std::make_unique<SegmentReader>(segment, time_in_playlist);
  return true;
}

void SegmentStorage::write_segment(std::string data) {
  segment_data.at(write_segment_data_index)->write_data(data);
}

void SegmentStorage::end_segment() {
  segment_data.at(write_segment_data_index)->end_data();
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s End segment", __FUNCTION__);
  write_segment_data_index = (write_segment_data_index + 1) % MAX_SEGMENTS;
}

bool SegmentStorage::has_download_item() {
  std::lock_guard<std::mutex> lock(data_lock);
  return current_segment_itr != segments.end();
}

void SegmentStorage::get_next_segment_reader(std::promise<SegmentReader*> promise) {
  std::lock_guard<std::mutex> lock(data_lock);
  std::unique_ptr<SegmentReader> &current_segment_reader = segment_data.at(read_segment_data_index);
  if (current_segment_reader) {
    promise.set_value(current_segment_reader.get());
    ++read_segment_data_index;
  } else {
    segment_reader_promise = std::move(promise);
  }
  // TODO: If we don't have the reader we have to save the promise somewhere
}

void SegmentStorage::download_next_segment() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of segments");
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(data_lock);
    download_cv.wait(lock, [&] {
      return quit_processing || can_download_segment();
    });

    if (quit_processing || no_more_data) {
      break;
    }
    lock.unlock();

    if (has_download_item()) {
      // TODO: Choose correct variant stream to get the segment from
      hls::Segment segment = current_segment_itr->details.at(0);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of %d", segment.media_sequence);

      DataHelper data_helper;
      data_helper.aes_iv = segment.aes_iv;
      data_helper.aes_uri = segment.aes_uri;
      data_helper.encrypted = segment.encrypted;
      data_helper.segment = segment;

      bool continue_download = start_segment(segment, current_segment_itr->time_in_playlist);
      if (!continue_download) {
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Demuxer says not to download");
        continue;
      }
      std::string url = segment.get_url();
      if (url.find("http") != std::string::npos) {
        downloader->download(url, segment.byte_offset, segment.byte_length,
            [&](std::string data) -> bool {
              this->process_data(data_helper, data);
              if (data_lock.try_lock()) {
                if (quit_processing) {
                  data_lock.unlock();
                  return false;
                } else {
                  data_lock.unlock();
                }
              }
              return true;
        });
      } else {
        FileDownloader file_downloader;
        std::string contents = file_downloader.download(url);
        this->process_data(data_helper, contents);
      }
      end_segment();
      ++current_segment_itr;
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Finished download of %d", segment.media_sequence);
    } else if (!live) {
        no_more_data = true;
        break;
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting download thread");
}

void SegmentStorage::process_data(DataHelper &data_helper, std::string data) {
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
  write_segment(data);
}


DownloadSegment::DownloadSegment(double time_in_playlist, hls::Segment segment, size_t index, size_t num_variant_streams) :
    duration(segment.duration),
    time_in_playlist(time_in_playlist),
    media_sequence(segment.media_sequence),
    details(num_variant_streams) {
  add_variant_segment(segment, index);
}

void DownloadSegment::add_variant_segment(hls::Segment segment, size_t index) {
  details[index] = segment;
}

void SegmentStorage::reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Reloading playlist %d", variant_stream->playlist.bandwidth);
  std::string url = variant_stream->playlist.get_url();
  std::string playlist_contents = downloader->download(url);
  if (!playlist_contents.empty()) {
   hls::MediaPlaylist new_media_playlist;
   new_media_playlist.set_url(url);
   new_media_playlist.load_contents(playlist_contents);
   size_t variant_stream_index = variant_stream - variants.begin();
   {
     std::lock_guard<std::mutex> lock(data_lock);
     std::vector<hls::Segment> playlist_segments = new_media_playlist.get_segments();
     for(auto& segment : playlist_segments) {
         if (segments.empty()) {
           segments.push_back(DownloadSegment(0.0, segment, variant_stream_index, variants.size()));
           variant_stream->last_segment_itr = segments.begin();
         } else {
           while (variant_stream->last_segment_itr != segments.end() &&
               segment.media_sequence <= variant_stream->last_segment_itr->media_sequence) {
             ++variant_stream->last_segment_itr;
           }
           if (variant_stream->last_segment_itr == segments.end()) {
             segments.push_back(DownloadSegment(segments.back().get_end_time(), segment, variant_stream_index, variants.size()));
             variant_stream->last_segment_itr = segments.begin();
           } else {
             variant_stream->last_segment_itr->add_variant_segment(segment, variant_stream_index);
           }
         }
     }
     // TODO: Check if current_segment_itr is at the end, if it is
     // we have to set it to the index of the newest element added
   }
  } else {
   xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Playlist %s is empty", url);
  }
}

void SegmentStorage::reload_playlist_thread() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting reload thread");
  std::vector<VariantStream>::iterator current_variant_stream = variants.begin();
  while(!quit_processing && (!all_loaded_once || live)) {
    std::unique_lock<std::mutex> lock(data_lock);
    reload_cv.wait_for(lock, std::chrono::milliseconds(RELOAD_DELAY_MS), [&] {
      return quit_processing;
    });

    if (quit_processing) {
      break;
    }
    lock.unlock();
    reload_playlist(current_variant_stream, downloader);
    ++current_variant_stream;
    if (current_variant_stream == variants.end()) {
      all_loaded_once = true;
      current_variant_stream = variants.begin();
      // TODO: Prune very old segments here
    }
    download_cv.notify_all();
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Exiting reload thread");
}


SegmentStorage::~SegmentStorage() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Deconstruct segment storage", __FUNCTION__);
  {
    std::lock_guard<std::mutex> lock(data_lock);
    quit_processing = true;
  }
  download_cv.notify_all();
  data_cv.notify_all();
  reload_cv.notify_all();
  download_thread.join();
  reload_thread.join();
}


