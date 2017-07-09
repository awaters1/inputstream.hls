/*
 * segment_storage.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>
#include <cmath>
#include <map>
#include <algorithm>

#include "globals.h"
#include "segment_storage.h"
#include "downloader/file_downloader.h"
#include "hls/decrypter.h"

#define LOGTAG                  "[SegmentStorage] "
#define STREAM_LOGTAG                  "[StreamSwitch] "
#define RELOAD_LOGTAG "[ReloadPlaylist] "

VariantStream::VariantStream(hls::MediaPlaylist playlist) : playlist(playlist) {

}

SegmentStorage::SegmentStorage(Downloader *downloader, hls::MasterPlaylist master_playlist) :
downloader(downloader),
quit_processing(false),
flush(false),
no_more_data(false),
live(true),
all_loaded_once(false),
valid_promise(false),
number_of_freezes(0),
time_in_buffer(0),
time_since_last_freeze_ms(1),
total_freeze_duration_ms(0) {
  for(auto &media_playlist : master_playlist.get_media_playlists()) {
    VariantStream stream(media_playlist);
    stream.last_segment_itr = segments.begin();
    variants.push_back(stream);
  }
  std::sort(variants.begin(), variants.end(), [](VariantStream l, VariantStream r) { return l.playlist.bandwidth < r.playlist.bandwidth; } );
  current_segment_itr = segments.end();
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Starting segment storage", __FUNCTION__);
  download_thread = std::thread(&SegmentStorage::download_next_segment, this);
  reload_thread = std::thread(&SegmentStorage::reload_playlist_thread, this);
  download_cv.notify_all();
}

bool SegmentStorage::can_download_segment() {
  // Called from a locked method
  return segment_data.size() < MAX_SEGMENTS;
}

std::shared_ptr<SegmentReader> SegmentStorage::start_segment(hls::Segment segment, double time_in_playlist, uint32_t chosen_variant_stream) {
  std::lock_guard<std::mutex> lock(data_lock);
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "%s Start segment %d at %f", __FUNCTION__,
      segment.media_sequence, time_in_playlist);
  std::shared_ptr<SegmentReader> segment_reader = std::make_shared<SegmentReader>(segment, time_in_playlist, chosen_variant_stream);
  if (valid_promise) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Start Segment media sequence: %d",
                    segment_reader->get_segment().media_sequence);
    segment_reader_promise.set_value(segment_reader);
    valid_promise = false;
  } else {
    segment_data.push_back(segment_reader);
  }
  return segment_reader;

}

bool SegmentStorage::has_download_item(uint32_t chosen_variant_stream) {
  std::lock_guard<std::mutex> lock(data_lock);
  return current_segment_itr != segments.end() && current_segment_itr->details.at(chosen_variant_stream).valid;
}

bool SegmentStorage::will_have_download_item(uint32_t chosen_variant_stream) {
  std::lock_guard<std::mutex> lock(data_lock);
  bool has_segment_after = false;
  for(auto itr = current_segment_itr; itr != segments.end(); ++itr) {
    has_segment_after |= itr->details.at(chosen_variant_stream).valid;
    if (has_segment_after) {
      break;
    }
  }
  return current_segment_itr == segments.end() || !has_segment_after;
}

void SegmentStorage::get_next_segment_reader(std::promise<std::shared_ptr<SegmentReader>> promise,
    uint64_t time_in_buffer, uint32_t total_freeze_duration_ms,
    uint32_t time_since_last_freeze_ms, uint32_t number_of_freezes) {
  std::lock_guard<std::mutex> lock(data_lock);
  this->time_in_buffer = time_in_buffer;
  this->total_freeze_duration_ms = total_freeze_duration_ms;
  this->time_since_last_freeze_ms = time_since_last_freeze_ms;
  this->number_of_freezes = number_of_freezes;
  xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "RL freeze duration: %d time_since_last: %d number: %d",
            total_freeze_duration_ms, time_since_last_freeze_ms, number_of_freezes);
  if (!segment_data.empty()) {
    std::shared_ptr<SegmentReader> segment_reader = std::move(segment_data.front());
    segment_data.pop_front();
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Get next media sequence: %d",
              segment_reader->get_segment().media_sequence);
    promise.set_value(std::move(segment_reader));
  } else {
    valid_promise = true;
    segment_reader_promise = std::move(promise);
    download_cv.notify_all();
  }
}

const int BANDWIDTH_BIN = 2000.0;

double quantify_bandwidth(double bandwidth_kbps) {
  return std::max(500, ((int) bandwidth_kbps / BANDWIDTH_BIN) * BANDWIDTH_BIN);
}

void SegmentStorage::download_next_segment() {
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of segments");
  uint32_t counter = 0;
  while(!quit_processing) {
    std::unique_lock<std::mutex> lock(data_lock);
    download_cv.wait(lock);

    if (quit_processing || no_more_data) {
      break;
    }

    if (!can_download_segment() || current_segment_itr == segments.end()) {
      continue;
    }

    // Create new stage
    Stage next_stage;
    next_stage.buffer_level_ms = time_in_buffer;
    next_stage.bandwidth_kbps = stage.bandwidth_kbps;
    next_stage.previous_quality_bps = stage.current_quality_bps;

    lock.unlock();

    // TODO: Choose correct variant stream to get the segment from
    // uint32_t chosen_variant_stream = counter > 3 ? 2 : 0; // rand() % variants.size();
    uint32_t chosen_variant_stream = 0;
    while (!has_download_item(chosen_variant_stream) && !will_have_download_item(chosen_variant_stream) && chosen_variant_stream < variants.size()) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Switching from variant stream %d because we will never load", chosen_variant_stream);
      ++chosen_variant_stream;
    }
    if (chosen_variant_stream >= variants.size()) {
      std::lock_guard<std::mutex> lock(data_lock);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "We cannot load segment ever %d", current_segment_itr->media_sequence);
      ++current_segment_itr;
    }

    next_stage.variant_stream_index = chosen_variant_stream;

    double variant_stream_kbps = variants.at(chosen_variant_stream).playlist.bandwidth / (double) 1024;

    double lowest_stream_bps = variants.at(0).playlist.bandwidth;
    uint32_t lowest_stream_index = 0;
    for(uint32_t i = 0; i < variants.size(); ++i) {
      if (variants.at(i).playlist.bandwidth <= lowest_stream_bps) {
        lowest_stream_bps = variants.at(i).playlist.bandwidth;
        lowest_stream_index = i;
      }
    }
    double lowest_stream_kbps = lowest_stream_bps / (double) 1024;

    // Reward
    double current_bandwidth = quantify_bandwidth(stage.bandwidth_kbps);
    double b_opt_ms = MAX_BUFFER_MS * 2.0 / 3.0;
    double num = (1.0 + (next_stage.buffer_level_ms / b_opt_ms));
    double denum = (3.0 - (BANDWIDTH_BIN / lowest_stream_kbps));
    double r_quality = -1.5 * std::fabs(next_stage.bandwidth_kbps * (num / denum) - variant_stream_kbps);
    double r_switches = -std::fabs(next_stage.previous_quality_bps / 1024.0 - variant_stream_kbps);
    double r_freeze = 0;
    double bw = variant_stream_kbps / current_bandwidth;
    if (chosen_variant_stream == lowest_stream_index) {
      r_freeze = -100 * fabs(bw * (std::exp(total_freeze_duration_ms / 10000.0) / std::log(time_since_last_freeze_ms + 1)));
    } else {
      r_freeze = -100 * fabs(bw * (std::exp(number_of_freezes + total_freeze_duration_ms / 10000.0) / std::log(time_since_last_freeze_ms + 1)));
    }
    double r_tot = r_quality + r_switches + r_freeze;

    if (has_download_item(chosen_variant_stream)) {
      xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "RL R: %f rQ: %f rS: %f rF: %f",
          r_tot, r_quality, r_switches, r_freeze);
      stage = next_stage;
      ++counter;
      lock.lock();
      // TODO: Potential optimization to check for flush right here
      hls::Segment segment = current_segment_itr->details.at(chosen_variant_stream);
      stage.current_quality_bps = variants.at(chosen_variant_stream).playlist.bandwidth;
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of %d at %f", segment.media_sequence, current_segment_itr->time_in_playlist);

      DataHelper data_helper;
      data_helper.aes_iv = segment.aes_iv;
      data_helper.aes_uri = segment.aes_uri;
      data_helper.encrypted = segment.encrypted;
      data_helper.total_bytes = 0;
      double time_in_playlist = current_segment_itr->time_in_playlist;

      lock.unlock();
      std::shared_ptr<SegmentReader> segment_reader = start_segment(segment, time_in_playlist, chosen_variant_stream);
      data_helper.segment_reader = segment_reader;
      std::string url = segment.get_url();
      std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
      if (url.find("http") != std::string::npos) {
        downloader->download(url, segment.byte_offset, segment.byte_length,
            [&](std::string data) -> bool {
              this->process_data(data_helper, data);
              if (data_lock.try_lock()) {
                if (quit_processing || flush) {
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
      // TODO :This should pass flush off to the demux and then up the chain instead of using
      // a boolean var in session
      segment_reader->end_data(flush);
      std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
      stage.download_time_ms = duration;
      stage.bandwidth_kbps = (data_helper.total_bytes * 1024 * 8) / (duration * 1000);
      downloader->record_bandwidth(stage.bandwidth_kbps);
      xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "Stage: buf: %f kpbs: %f prev_qual: %f curr_qual: %f dl_ms: %f",
          stage.buffer_level_ms, stage.bandwidth_kbps, stage.previous_quality_bps,
          stage.current_quality_bps, stage.download_time_ms);
      lock.lock();
      if (!flush) {
        ++current_segment_itr;
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Finished download of %d", segment.media_sequence);
      } else {
        segment_data.clear();
        flush = false;
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Flushed download of %d", segment.media_sequence);
      }
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
  data_helper.segment_reader->write_data(data);
  data_helper.total_bytes += data.length();
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

uint64_t SegmentStorage::get_total_duration() {
  std::lock_guard<std::mutex> lock(data_lock);
  double total_time = 0;
  for(auto it = segments.begin(); it != segments.end(); ++it) {
    total_time += it->duration;
  }
  return (uint64_t) total_time * 1000;
}

std::list<DownloadSegment>::iterator SegmentStorage::find_segment_at_time(double time_in_seconds) {
  std::lock_guard<std::mutex> lock(data_lock);
  double running_total(0);
  for(auto it = segments.begin(); it != segments.end(); ++it) {
    if (running_total >= time_in_seconds) {
      if (it != segments.begin()) {
        return --it;
      } else {
        return segments.begin();
      }
    }
    running_total += it->duration;
  }
  xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Unable to find segment at %f", time_in_seconds);
  return segments.end();
}

double SegmentStorage::seek_time(double desired_time) {
  if (segments.empty()) {
      // TODO: Wait for playlist
    std::unique_lock<std::mutex> lock(data_lock);
    download_cv.wait(lock, [&] {
      return quit_processing || can_download_segment();
    });
  }

  std::list<DownloadSegment>::iterator seek_to = find_segment_at_time(desired_time);
  if (seek_to == segments.end()) {
    return -1;
  } else {
    double new_time = seek_to->time_in_playlist;
    xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "seek to %+6.3f", new_time);
    {
      std::lock_guard<std::mutex> lock(data_lock);
      flush = true;
      segment_data.clear();
      current_segment_itr = seek_to;
    }
    download_cv.notify_all();
    return new_time;
  }
}

void SegmentStorage::reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader) {
  xbmc->Log(ADDON::LOG_DEBUG, RELOAD_LOGTAG "Reloading playlist %d", variant_stream->playlist.bandwidth);
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
           current_segment_itr = segments.begin();
           variant_stream->last_segment_itr = segments.begin();
         } else {
           if (variant_stream->last_segment_itr == segments.end()) {
               variant_stream->last_segment_itr = segments.begin();
           }
           while (variant_stream->last_segment_itr != segments.begin() &&
               segment.media_sequence < variant_stream->last_segment_itr->media_sequence) {
             --variant_stream->last_segment_itr;
           }
           while (variant_stream->last_segment_itr != segments.end() &&
                segment.media_sequence > variant_stream->last_segment_itr->media_sequence) {
              ++variant_stream->last_segment_itr;
           }
           if (variant_stream->last_segment_itr == segments.end() ||
               variant_stream->last_segment_itr->media_sequence != segment.media_sequence) {
               variant_stream->last_segment_itr = segments.insert(variant_stream->last_segment_itr,
                             DownloadSegment(segments.back().get_end_time(), segment, variant_stream_index, variants.size()));
             if (current_segment_itr == segments.end()) {
               current_segment_itr = --segments.end();
             }
           } else {
             variant_stream->last_segment_itr->add_variant_segment(segment, variant_stream_index);
           }
         }
     }
   }
   download_cv.notify_all();
  } else {
   xbmc->Log(ADDON::LOG_DEBUG, RELOAD_LOGTAG "Playlist %s is empty", url.c_str());
  }
}

void SegmentStorage::reload_playlist_thread() {
  xbmc->Log(ADDON::LOG_DEBUG, RELOAD_LOGTAG "Starting reload thread, %f bandwidth", downloader->get_average_bandwidth());
  std::vector<VariantStream>::iterator current_variant_stream = variants.begin();
  for(auto it = variants.begin(); it != variants.end(); ++it) {
    if (it->playlist.bandwidth <= downloader->get_average_bandwidth() &&
          it->playlist.bandwidth > current_variant_stream->playlist.bandwidth) {
      current_variant_stream = it;
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, RELOAD_LOGTAG "Reload starting with %d", current_variant_stream->playlist.bandwidth);
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
  xbmc->Log(ADDON::LOG_DEBUG, RELOAD_LOGTAG "Exiting reload thread");
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


