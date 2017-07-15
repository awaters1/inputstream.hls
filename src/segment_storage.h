#pragma once
/*
 *      Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <unordered_map>
#include <future>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include "hls/HLS.h"
#include "downloader/downloader.h"
#include "hls/SegmentReader.h"

const int RELOAD_DELAY_MS = 1000;
const size_t MAX_SEGMENTS =  6;
const int LOWEST_BANDWIDTH = 500;
const double ALPHA = 0.5;
const double GAMMA = 0.5;
const double DELTA = 1 / 7.0;// TODO: Should be based on # of variants
const double INVERSE_SENSITIVITY = 10;
const double TEMPERATURE = 0.5;

struct DataHelper {
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  std::shared_ptr<SegmentReader> segment_reader;
  size_t total_bytes;
};

class DownloadSegment {
public:
  DownloadSegment(double time_in_playlist, hls::Segment segment, size_t index, size_t num_variant_streams);
  void add_variant_segment(hls::Segment segment, size_t index);
  double get_end_time() {
    return time_in_playlist + duration;
  };
public:
  double duration;
  double time_in_playlist;
  uint32_t media_sequence;

  std::vector<hls::Segment> details;
};

class VariantStream {
public:
  VariantStream(hls::MediaPlaylist playlist);
  hls::MediaPlaylist playlist;
  // Points to the last added segment
  std::list<DownloadSegment>::iterator last_segment_itr;
};

struct Reward {
  double total;
  double quality;
  double switches;
  double freeze;
};

double quantify_bandwidth(double bandwidth_kbps);

class State {
public:
  State(double buffer_level_ms, double bandwidth_kbps, double previous_quality_bps) :
    buffer_level_ms(buffer_level_ms),
    bandwidth_kbps(bandwidth_kbps),
    previous_quality_bps(previous_quality_bps) {};
  bool operator==(const State &other) const {
    return get_buffer_level_s() == other.get_buffer_level_s() &&
        get_bandwidth_kbps() == other.get_bandwidth_kbps() &&
        get_previous_quality_kbps() == other.get_previous_quality_kbps();
  }
  uint32_t get_buffer_level_s() const {
    return buffer_level_ms / 1000;
  }
  uint32_t get_bandwidth_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(bandwidth_kbps));
  }
  uint32_t get_previous_quality_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(previous_quality_bps / 1024.0));
  }
  double buffer_level_ms;
  double bandwidth_kbps;
  double previous_quality_bps;
};

class Action {
public:
  Action(double current_quality_bps) : current_quality_bps(current_quality_bps) {};
  bool operator==(const Action &other) const {
    return get_current_quality_kbps() == other.get_current_quality_kbps();
  }
  uint32_t get_current_quality_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(current_quality_bps / 1024.0));
  }
  double current_quality_bps;
};

class StateAction {
public:
  StateAction(State state, Action action) : state(state), action(action) {};
  State state;
  Action action;
  bool operator==(const StateAction &other) const {
    return state == other.state && action == other.action;
  }
};

class Stage {
public:
  Stage() : buffer_level_ms(0), bandwidth_kbps(0),
    previous_quality_bps(0), current_quality_bps(0), download_time_ms(0),
    variant_stream_index(0) {};
  bool operator==(const Stage &other) const {
    return get_buffer_level_s() == other.get_buffer_level_s() &&
        get_bandwidth_kbps() == other.get_bandwidth_kbps() &&
        get_previous_quality_kbps() == other.get_previous_quality_kbps() &&
        get_current_quality_kbps() == other.get_current_quality_kbps();
  }
  uint32_t get_buffer_level_s() const {
    return buffer_level_ms / 1000;
  }
  uint32_t get_bandwidth_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(bandwidth_kbps));
  }
  uint32_t get_previous_quality_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(previous_quality_bps / 1024.0));
  }
  uint32_t get_current_quality_kbps() const {
    return static_cast<uint32_t>(quantify_bandwidth(current_quality_bps / 1024.0));
  }
  double buffer_level_ms;
  double bandwidth_kbps;
  double previous_quality_bps;
  double current_quality_bps;
  double download_time_ms; // filled in after the stage is done
  uint32_t variant_stream_index;
  State get_state() {
    return State(buffer_level_ms, bandwidth_kbps, previous_quality_bps);
  };
  Action get_action() {
    return Action(current_quality_bps);
  };
  StateAction get_state_action() {
    return StateAction(get_state(), get_action());
  };
};

namespace std {
  template <>
  struct hash<StateAction> {
    std::size_t operator()(const StateAction& stage) const {
      return (((hash<uint32_t>()(stage.state.get_buffer_level_s()))
          ^ (hash<uint32_t>()(stage.state.get_bandwidth_kbps()) << 1) >> 1)
          ^ (hash<uint32_t>()(stage.state.get_previous_quality_kbps()) << 1 ) >> 1)
          ^ (hash<uint32_t>()(stage.action.get_current_quality_kbps()) << 1);
    }
  };
  template <>
  struct hash<State> {
    std::size_t operator()(const State& state) const {
      return ((hash<uint32_t>()(state.get_buffer_level_s()))
          ^ (hash<uint32_t>()(state.get_bandwidth_kbps()) << 1) >> 1)
          ^ (hash<uint32_t>()(state.get_previous_quality_kbps()) << 1);
    }
  };
}

class SegmentStorage {
public:
  SegmentStorage(Downloader *downloader, hls::MasterPlaylist master_playlist,
      std::unordered_map<StateAction, double> q_map, std::unordered_map<State, double> explore_map);
  ~SegmentStorage();
  void get_next_segment_reader(std::promise<std::shared_ptr<SegmentReader>> promise, uint64_t time_in_buffer,
      uint32_t total_freeze_duration_ms, uint32_t time_since_last_freeze_ms, uint32_t number_of_freezes);
  double seek_time(double desired_time);
  uint64_t get_total_duration();
  std::unordered_map<StateAction, double> get_q_map() { return q_map; };
  std::unordered_map<State, double> get_explore_map() { return explore_map; };
private:
  std::shared_ptr<SegmentReader> start_segment(hls::Segment segment, double time_in_playlist, uint32_t chosen_variant_stream);
  bool can_download_segment();
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
  bool has_download_item(uint32_t chosen_variant_stream);
  bool will_have_download_item(uint32_t chosen_variant_stream);
private:
  Reward calculate_reward(Stage stage);
  Reward calculate_reward(State state, Action action, uint32_t variant_stream_index);
  uint32_t best_action(State state);
  void normalize_q_map();
private:
  std::list<std::shared_ptr<SegmentReader>> segment_data;
  bool valid_promise;
  std::promise<std::shared_ptr<SegmentReader>> segment_reader_promise;
  bool quit_processing;
  bool no_more_data;
  bool flush;
  Downloader *downloader;

  std::unordered_map<std::string, std::string> aes_uri_to_key;

  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;
  std::mutex data_lock;
  std::condition_variable data_cv;

private:
  std::list<DownloadSegment>::iterator find_segment_at_time(double time_in_seconds);
  void reload_playlist_thread();
  void reload_playlist(std::vector<VariantStream>::iterator variant_stream, Downloader  *downloader);
  bool live;
  bool all_loaded_once;
  std::vector<VariantStream> variants;
  std::list<DownloadSegment> segments;
  std::list<DownloadSegment>::const_iterator current_segment_itr;
  // Reload thread
  std::thread reload_thread;
  std::condition_variable reload_cv;
private:
  uint64_t time_in_buffer;
  uint32_t total_freeze_duration_ms;
  uint32_t time_since_last_freeze_ms;
  uint32_t number_of_freezes;
  Stage stage;
  std::unordered_map<StateAction, double> q_map;
  std::unordered_map<StateAction, double> normalized_q_map;
  std::unordered_map<State, double> explore_map;
};
