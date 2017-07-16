/*
 * segment_storage.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <cstring>
#include <cmath>
#include <map>
#include <algorithm>
#include <limits>

#include "globals.h"
#include "segment_storage.h"
#include "downloader/file_downloader.h"
#include "hls/decrypter.h"

#define LOGTAG                  "[SegmentStorage] "
#define STREAM_LOGTAG                  "[StreamSwitch] "
#define RL_LOGTAG                  "[RL] "
#define RELOAD_LOGTAG "[ReloadPlaylist] "

VariantStream::VariantStream(hls::MediaPlaylist playlist) : playlist(playlist) {

}

SegmentStorage::SegmentStorage(Downloader *downloader, hls::MasterPlaylist master_playlist,
    std::unordered_map<StateAction, double> q_map,
    std::unordered_map<State, double> explore_map) :
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
total_freeze_duration_ms(0),
q_map(q_map),
explore_map(explore_map){
  for(auto &media_playlist : master_playlist.get_media_playlists()) {
    VariantStream stream(media_playlist);
    stream.last_segment_itr = segments.begin();
    variants.push_back(stream);
  }
  std::sort(variants.begin(), variants.end(), [](VariantStream l, VariantStream r) { return l.playlist.bandwidth < r.playlist.bandwidth; } );
  current_segment_itr = segments.end();
  stage.bandwidth_kbps = downloader->get_average_bandwidth() / 1024.0;
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
  return current_segment_itr != segments.end() && chosen_variant_stream < variants.size() &&
      current_segment_itr->details.at(chosen_variant_stream).valid;
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

double quantify_bandwidth(double bandwidth_kbps) {
  double quantified_bandwidth;
  if (bandwidth_kbps < 5000) {
    quantified_bandwidth = ((int) bandwidth_kbps / 500) * 500;
  } else if (bandwidth_kbps < 10000) {
    quantified_bandwidth = ((int) bandwidth_kbps / 1000) * 1000;
  } else {
    quantified_bandwidth = ((int) bandwidth_kbps / 2000) * 2000;
  }
  return std::max(500.0, quantified_bandwidth);
}

Reward SegmentStorage::calculate_reward(Stage stage) {
  return calculate_reward(stage.get_state(), stage.get_action(), stage.variant_stream_index);
}

Reward SegmentStorage::calculate_reward(State state, Action action, uint32_t variant_stream_index) {
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
  double b_opt_s = MAX_BUFFER_MS * 2.0 / 3.0 / 1000.0;
  /*
  double bw_mod = 1;
  if (state.get_buffer_level_s() < b_opt_s) {
    bw_mod = state.get_buffer_level_s() / b_opt_s;
  }
  double r_quality = state.get_bandwidth_kbps() * bw_mod - action.get_current_quality_kbps();
  double r_switches;
  if (state.get_previous_quality_kbps() < action.get_current_quality_kbps()) {
    r_switches = -0.5 *std::fabs((double) state.get_previous_quality_kbps() - action.get_current_quality_kbps());
  } else {
    r_switches = -std::fabs((double) state.get_previous_quality_kbps() - action.get_current_quality_kbps());
  }
  */
  double num = (1.0 + (state.get_buffer_level_s() / b_opt_s));
  double denum = (3 - (LOWEST_BANDWIDTH / lowest_stream_kbps));
  double r_quality = -1.5 * std::fabs(state.get_bandwidth_kbps() * (num / denum) - action.get_current_quality_kbps());
  double r_switches = -std::fabs((double) state.get_previous_quality_kbps() - action.get_current_quality_kbps());
  double r_freeze = 0;
  double bw = action.get_current_quality_kbps() / state.get_bandwidth_kbps();
  if (stage.variant_stream_index == lowest_stream_index) {
    r_freeze = -100 * fabs(bw * (std::exp(total_freeze_duration_ms / 10000.0) / std::log(time_since_last_freeze_ms + 1)));
  } else {
    r_freeze = -100 * fabs(bw * (std::exp(number_of_freezes + total_freeze_duration_ms / 10000.0) / std::log(time_since_last_freeze_ms + 1)));
  }
  double r_tot = r_quality + r_switches + r_freeze;
  Reward reward;
  reward.total = r_tot;
  reward.quality = r_quality;
  reward.switches = r_switches;
  reward.freeze = r_freeze;
  return reward;
}

void SegmentStorage::normalize_q_map() {
  double min = std::numeric_limits<double>::max();
  double max = std::numeric_limits<double>::min();
  for(auto it : q_map) {
    min = std::min(min, it.second);
    max = std::max(max, it.second);
  }
  for(auto &it : q_map) {
    normalized_q_map[it.first] = (it.second - min) / (max - min);
  }
}

uint32_t SegmentStorage::best_action(State state) {
  uint32_t chosen_variant_stream = 0;
  for(size_t i = 0; i < variants.size(); ++i) {
    Action action(variants.at(i).playlist.bandwidth);
    if (q_map.find(StateAction(state, action)) == q_map.end()) {
      Reward reward = calculate_reward(state, action, i);
      q_map[StateAction(state, action)] = reward.quality + reward.switches;
    }
  }
  normalize_q_map();
  size_t max = -1;
  double max_q;
  for(size_t i = 0; i < variants.size(); ++i) {
    Action action(variants.at(i).playlist.bandwidth);
    double q_value = q_map[StateAction(state, action)];
    if (max == -1 || q_value >= max_q) {
      max_q = q_value;
      max = i;
    }
  }
  xbmc->Log(ADDON::LOG_DEBUG, RL_LOGTAG "Best action is %d with Q_value: %f bw: %d, buff: %d, prev_qual: %d",
                            max, max_q, state.get_bandwidth_kbps(), state.get_buffer_level_s(), state.get_previous_quality_kbps());
  chosen_variant_stream = max;
  return chosen_variant_stream;
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

    State state = stage.get_state();

    lock.unlock();

    bool qlearn = true;
    bool exploring = true;

    uint32_t chosen_variant_stream = 0;
    if (qlearn) {
      chosen_variant_stream = best_action(state);
      double epsilon = (double) rand() / RAND_MAX;
      if (explore_map.find(state) == explore_map.end()) {
        explore_map[state] = 1;
      }
      if (epsilon < explore_map[state]) {
        xbmc->Log(ADDON::LOG_DEBUG, RL_LOGTAG "Doing softmax epsilon: %f, explore: %f",
                  epsilon, explore_map[state]);
        double reward_sum = 0;
        for(size_t i = 0; i < variants.size(); ++i) {
          Action action(variants.at(i).playlist.bandwidth);
          reward_sum += exp(normalized_q_map[StateAction(state, action)] / TEMPERATURE);
        }
        double max_probability = 0;
        for(size_t i = 0; i < variants.size(); ++i) {
          Action action(variants.at(i).playlist.bandwidth);
          double probability = exp(normalized_q_map[StateAction(state, action)] / TEMPERATURE) / reward_sum;
          if (probability >= max_probability) {
            max_probability = probability;
            chosen_variant_stream = i;
          }
          xbmc->Log(ADDON::LOG_DEBUG, RL_LOGTAG "stream %d with probability %f, kbps: %d",
                                    i, probability, action.get_current_quality_kbps());
        }
        xbmc->Log(ADDON::LOG_DEBUG, RL_LOGTAG "Chose stream %d with probability %f",
                          chosen_variant_stream, max_probability);
      } else {
        // chosen_variant_stream is already the best action
      }
    } else {
      for(chosen_variant_stream = 0; chosen_variant_stream < variants.size() - 1; ++chosen_variant_stream) {
        if (variants.at(chosen_variant_stream).playlist.bandwidth >= stage.bandwidth_kbps * 1024) {
          break;
        }
      }
      chosen_variant_stream = 0;
    }
    // TODO: Testisg flashing
    chosen_variant_stream = rand() % (3);
    uint32_t original_chosen_stream = chosen_variant_stream;

    while (!has_download_item(chosen_variant_stream) && !will_have_download_item(chosen_variant_stream) && chosen_variant_stream < variants.size()) {
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Switching from variant stream %d because we will never load", chosen_variant_stream);
      ++chosen_variant_stream;
    }
    if (chosen_variant_stream >= variants.size()) {
      std::lock_guard<std::mutex> lock(data_lock);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "We cannot load segment ever %d", current_segment_itr->media_sequence);
      ++current_segment_itr;
      chosen_variant_stream = original_chosen_stream;
    }

    stage.current_quality_bps = variants.at(chosen_variant_stream).playlist.bandwidth;
    stage.variant_stream_index = chosen_variant_stream;

    if (has_download_item(chosen_variant_stream)) {
      ++counter;
      lock.lock();
      // TODO: Potential optimization to check for flush right here
      hls::Segment segment = current_segment_itr->details.at(chosen_variant_stream);
      xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Starting download of %d at %f stream: %d", segment.media_sequence, current_segment_itr->time_in_playlist, chosen_variant_stream);

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
      bool failed = false;
      if (url.find("http") != std::string::npos) {
        downloader->download(url, segment.byte_offset, segment.byte_length,
            [&](std::string data) -> bool {
              if (!data.empty()) {
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
              } else {
                failed = true;
              }
        });
      } else {
        FileDownloader file_downloader;
        std::string contents = file_downloader.download(url);
        this->process_data(data_helper, contents);
      }
      segment_reader->end_data(flush);
      std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
      stage.download_time_ms = duration;
      // TODO: Shouldn't update stage here
      stage.bandwidth_kbps = (data_helper.total_bytes * 1024 * 8) / (duration * 1000);
      downloader->record_bandwidth(stage.bandwidth_kbps);
      xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "Stage: buf: %f kpbs: %f prev_qual: %f curr_qual: %f dl_ms: %f",
          stage.buffer_level_ms, stage.bandwidth_kbps, stage.previous_quality_bps,
          stage.current_quality_bps, stage.download_time_ms);
      lock.lock();
      if (!failed) {
        if (!flush) {
          ++current_segment_itr;
          xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Finished download of %d", segment.media_sequence);
        } else {
          segment_data.clear();
          flush = false;
          xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Flushed download of %d", segment.media_sequence);
        }
      } else {
        xbmc->Log(ADDON::LOG_DEBUG, LOGTAG "Failed download of %d, trying again", segment.media_sequence);
      }

      // Q Learn
      Reward reward = calculate_reward(stage);

      Stage next_stage;
      next_stage.buffer_level_ms = time_in_buffer;
      next_stage.bandwidth_kbps = stage.bandwidth_kbps;
      next_stage.previous_quality_bps = stage.current_quality_bps;

      State next_state = next_stage.get_state();

      uint32_t next_best_stream = best_action(next_state);
      Action next_best_action(variants.at(next_best_stream).playlist.bandwidth);

      double old_q = normalized_q_map[stage.get_state_action()];
      q_map[stage.get_state_action()] = (1 - ALPHA) * q_map[stage.get_state_action()] + ALPHA *
          (reward.total + GAMMA * q_map[StateAction(next_state, next_best_action)]);
      normalize_q_map();

      // TODO: This uses q_values that were normalized differently
      double change_in_q = normalized_q_map[stage.get_state_action()] - old_q;
      explore_map[state] = DELTA * (
            (1 - exp((-fabs(ALPHA * change_in_q) / INVERSE_SENSITIVITY))) /
            (1 + exp((-fabs(ALPHA * change_in_q) / INVERSE_SENSITIVITY)))
          ) + (1 - DELTA) * explore_map[state];

      xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "RL R: %f rQ: %f rS: %f rF: %f Q: %f change: %f",
          reward.total, reward.quality, reward.switches, reward.freeze, q_map[stage.get_state_action()],
          change_in_q);
      stage = next_stage;

      // Debug
      std::vector<std::pair<StateAction, double>> q_vec(q_map.begin(), q_map.end());
      std::sort(q_vec.begin(), q_vec.end(), [](std::pair<StateAction, double> l, std::pair<StateAction, double> r) -> bool {
        if (l.first.state.get_buffer_level_s() < r.first.state.get_buffer_level_s()) {
          return true;
        } else if (l.first.state.get_buffer_level_s() > r.first.state.get_buffer_level_s()) {
          return false;
        } else if (l.first.state.get_bandwidth_kbps() < r.first.state.get_bandwidth_kbps()) {
          return true;
        } else if (l.first.state.get_bandwidth_kbps() > r.first.state.get_bandwidth_kbps()) {
          return false;
        } else if (l.first.state.get_previous_quality_kbps() < r.first.state.get_previous_quality_kbps()) {
          return true;
        } else if (l.first.state.get_previous_quality_kbps() > r.first.state.get_previous_quality_kbps()) {
          return false;
        } else if (l.first.action.get_current_quality_kbps() > r.first.action.get_current_quality_kbps()) {
          return true;
        } else {
          return false;
        }
      });
      for(auto it : q_vec) {
        xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "buffer_s: %d bw_kbps: %d prev_qual: %d curr_qual: %d  Q: %f",
            it.first.state.get_buffer_level_s(), it.first.state.get_bandwidth_kbps(), it.first.state.get_previous_quality_kbps(),
            it.first.action.get_current_quality_kbps(), it.second);
      }
      std::vector<std::pair<State, double>> e_vec(explore_map.begin(), explore_map.end());
      std::sort(e_vec.begin(), e_vec.end(), [](std::pair<State, double> l, std::pair<State, double> r) -> bool {
        if (l.first.get_buffer_level_s() < r.first.get_buffer_level_s()) {
          return true;
        } else if (l.first.get_buffer_level_s() > r.first.get_buffer_level_s()) {
          return false;
        } else if (l.first.get_bandwidth_kbps() < r.first.get_bandwidth_kbps()) {
          return true;
        } else if (l.first.get_bandwidth_kbps() > r.first.get_bandwidth_kbps()) {
          return false;
        } else if (l.first.get_previous_quality_kbps() < r.first.get_previous_quality_kbps()) {
          return true;
        } else {
          return false;
        }
      });
      for(auto it : e_vec) {
        xbmc->Log(ADDON::LOG_DEBUG, STREAM_LOGTAG "buffer_s: %d bw_kbps: %d prev_qual: %d E: %f",
            it.first.get_buffer_level_s(), it.first.get_bandwidth_kbps(), it.first.get_previous_quality_kbps(), it.second);
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
      // TODO: Wait for playlist before attempting to seek, this usually
    // happens resuming from a spot
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


