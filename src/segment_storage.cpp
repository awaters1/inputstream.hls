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
segment_locks(MAX_SEGMENTS),
downloader(downloader),
quit_processing(false),
no_more_data(false),
live(true),
all_loaded_once(false) {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting segment storage", __FUNCTION__);
  download_thread = std::thread(&SegmentStorage::download_next_segment, this);
  download_cv.notify_all();
}

bool SegmentStorage::can_download_segment() {
  // Don't need to lock data_lock because it is locked by the download thread
  std::lock_guard<std::mutex> segment_lock(segment_locks.at(write_segment_data_index));
  SegmentData &current_segment_data = segment_data.at(write_segment_data_index);
  if (current_segment_data.can_overwrite == false) {
    return false;
  }
  return true;
}

bool SegmentStorage::start_segment(hls::Segment segment) {
  std::lock_guard<std::mutex> lock(data_lock);
  std::lock_guard<std::mutex> segment_lock(segment_locks.at(write_segment_data_index));
  SegmentData &current_segment_data = segment_data.at(write_segment_data_index);
  if (current_segment_data.can_overwrite == false) {
    return false;
  } else if (current_segment_data.segment.valid && write_segment_data_index == read_segment_data_index) {
    // We are overwriting an existing element so increment read pointer
    read_segment_data_index = (read_segment_data_index + 1) % MAX_SEGMENTS;
  }
  offset += current_segment_data.contents.length();
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Start segment %d at %d", __FUNCTION__,
      segment.media_sequence, offset);
  current_segment_data.start_offset = offset;
  current_segment_data.segment = segment;
  current_segment_data.contents.clear();
  current_segment_data.can_overwrite = false;
  current_segment_data.finished = false;
  return true;
}

void SegmentStorage::write_segment(hls::Segment segment, std::string data) {
  std::lock_guard<std::mutex> lock(segment_locks.at(write_segment_data_index));
  if (segment_data.at(write_segment_data_index).segment == segment) {
    segment_data.at(write_segment_data_index).contents += data;
    segment_data.at(write_segment_data_index).can_overwrite = false;
    // xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Wrote %d bytes, %d total bytes", __FUNCTION__, data.length(),
    //    segment_data.at(write_segment_data_index).contents.length());
    data_cv.notify_all();
  }
}

void SegmentStorage::end_segment(hls::Segment segment) {
  std::lock_guard<std::mutex> lock(segment_locks.at(write_segment_data_index));
  if (segment_data.at(write_segment_data_index).segment == segment) {
    segment_data.at(write_segment_data_index).finished = true;
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s End segment %d at %d with %d bytes", __FUNCTION__,
        segment.media_sequence, offset, segment_data.at(write_segment_data_index).contents.length());
    write_segment_data_index = (write_segment_data_index + 1) % MAX_SEGMENTS;
    data_cv.notify_all();
  }
}

size_t SegmentStorage::get_size() {
  size_t size(0);
  for(size_t i = 0; i < segment_data.size(); ++i) {
    std::lock_guard<std::mutex> lock(segment_locks.at(i));
    SegmentData &s = segment_data.at(i);
    size += s.contents.length();
  }
  return size;
}

hls::Segment SegmentStorage::read(uint64_t pos, size_t &size, uint8_t * const destination, size_t min_read) {
  std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
  size_t desired_size = size;
  size_t data_read = 0;
  hls::Segment segment = read_impl(pos, size, destination);
  if (size >= min_read) {
    return segment;
  }
  {
    std::lock_guard<std::mutex> lock(data_lock);
    if (size < min_read && no_more_data) {
      return segment;
    }
  }
  data_read += size;
  // Size == 0
  while(data_read < min_read) {
    // xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Waiting for data to be downloaded, pos %d size %d bytes_read %d", __FUNCTION__,pos, size, bytes_read);
    download_cv.notify_all();
    std::unique_lock<std::mutex> lock(data_lock);
    data_cv.wait_for(lock, std::chrono::milliseconds(100));
    if (quit_processing || no_more_data) {
      return segment;
    }
    lock.unlock();
    size = desired_size - data_read;
    segment = read_impl(pos + data_read, size, destination + data_read);
    data_read += size;
    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    if (duration >= READ_TIMEOUT_MS) {
        xbmc->Log(ADDON::LOG_ERROR, LOGTAG "%s Read timeout", __FUNCTION__);
        break;
    }
  }
  size = data_read;
  if (!segment.valid) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s segment is invalid", __FUNCTION__);
  }
  return segment;
}

hls::Segment SegmentStorage::read_impl(uint64_t pos, size_t &size, uint8_t * const destination) {
  std::lock_guard<std::mutex> lock(data_lock);
  uint64_t destination_offset = 0;
  uint32_t current_read_segment_index = read_segment_data_index;
  uint64_t next_offset = offset;
  hls::Segment first_segment;
  uint64_t data_read = 0;
  uint64_t wanted_data = size;
  while(size > 0) {
    uint64_t relative_offset;
    if (pos >= next_offset) {
      relative_offset = pos - next_offset;
    } else {
      relative_offset = 0; // start at beginning of segment
    }
    std::lock_guard<std::mutex> segment_lock(segment_locks.at(current_read_segment_index));
    SegmentData &current_segment = segment_data.at(current_read_segment_index);
    if (!current_segment.segment.valid) {
      break;
    }
    bool go_to_next_segment = false;
    if (relative_offset < current_segment.contents.length()) {
      if (!first_segment.valid) {
        first_segment = current_segment.segment;
      }
      size_t data_left_in_segment = current_segment.contents.length() - relative_offset;
      size_t data_to_read_from_segment;
      if (data_left_in_segment < size) {
        data_to_read_from_segment = data_left_in_segment;
        next_offset += current_segment.contents.length();
      } else {
        data_to_read_from_segment = size;
      }
      std::memcpy(destination + destination_offset,
          current_segment.contents.c_str() + relative_offset, data_to_read_from_segment);
      destination_offset += data_to_read_from_segment;
      size -= data_to_read_from_segment;
      data_read += data_to_read_from_segment;
    } else if (current_segment.finished) {
      // We read all of the data in this segment so it is safe to overwrite
      if (!current_segment.can_overwrite) {
        // data_lock is locked up top
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Triggering download", __FUNCTION__);
        current_segment.can_overwrite = true;
        download_cv.notify_all();
      }
      next_offset += current_segment.contents.length();
    } else {
      // The segment we are reading from isn't finished so we cannot read anymore
      break;
    }
    current_read_segment_index = (current_read_segment_index + 1)% MAX_SEGMENTS;
    if (current_read_segment_index == read_segment_data_index) {
      // We looped around
      break;
    }
  }
  if (data_read < wanted_data) {
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Not enough data read pos: %d, data read %d data wanted %d", __FUNCTION__, pos, data_read, wanted_data);
  } else {
//    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Read %d bytes at %d", __FUNCTION__, data_read, pos);
  }
  size = data_read;
  if (!first_segment.valid) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s First segment is invalid", __FUNCTION__);
  }
  return first_segment;
}

bool SegmentStorage::has_download_item() {
  std::lock_guard<std::mutex> lock(data_lock);
  return current_segment_itr != segments.end();
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
      hls::Segment segment = current_segment_itr->details.at(0);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of %d", segment.media_sequence);

      DataHelper data_helper;
      data_helper.aes_iv = segment.aes_iv;
      data_helper.aes_uri = segment.aes_uri;
      data_helper.encrypted = segment.encrypted;
      data_helper.segment = segment;

      bool continue_download = start_segment(segment);
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
      end_segment(segment);
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
  write_segment(data_helper.segment, data);
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


