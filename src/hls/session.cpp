/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

#include "decrypter.h"

#include "session.h"

bool hls::ActiveSegment::write_data(const void *buffer, size_t buffer_size) {
  segment_buffer += std::string((const char *)buffer, buffer_size);
  return true;
}

void hls::ActiveSegment::extract_streams() {
  for(std::vector<TSDemux::STREAM_PKT*>::iterator it = packets.begin(); it != packets.end(); ++it) {
    if ((*it)->streamChange) {
      TSDemux::ElementaryStream *es = demux->get_elementary_stream((*it)->pid);
      Stream stream;
      stream.stream_id = (*it)->pid;
      stream.codec_name = es->GetStreamCodecName();
      stream.channels = es->stream_info.channels;
      stream.sample_rate = es->stream_info.sample_rate;
      stream.bit_rate = es->stream_info.bit_rate;
      stream.bits_per_sample = es->stream_info.bits_per_sample;
      streams.push_back(stream);
    }
  }
}

void hls::ActiveSegment::create_demuxer(std::string aes_key) {
  if (segment.encrypted) {
    segment_buffer = decrypt(aes_key, segment.aes_iv, segment_buffer);
  }
  create_demuxer();
}

void hls::ActiveSegment::create_demuxer() {
  demux = new Demux(segment_buffer, 0);
  TSDemux::STREAM_PKT* pkt = 0;
  while (pkt = demux->get_next_pkt()) {
      unsigned char *data = new unsigned char[pkt->size];
      memcpy(data, pkt->data, pkt->size);
      pkt->data = data;
      if (pkt->streamChange) {
        // Insert a fake streamChange packet
        TSDemux::STREAM_PKT* stream_change_pkt = new TSDemux::STREAM_PKT();
        stream_change_pkt->streamChange = true;
        stream_change_pkt->pid = pkt->pid;
        packets.push_back(stream_change_pkt);
        pkt->streamChange = false;
      }
      packets.push_back(pkt);
  }
  extract_streams();
}

hls::ActiveSegment::~ActiveSegment() {
  std::cout << "Deleting active segment for " << segment.media_sequence << "\n";
  if (demux) {
    delete demux;
  }
  for(std::vector<TSDemux::STREAM_PKT*>::iterator it = packets.begin(); it != packets.end(); ++it) {
      delete [] (*it)->data;
      delete *it;
  }
}

TSDemux::STREAM_PKT* hls::ActiveSegment::get_next_pkt() {
  // return demux->get_next_pkt();

  if (packet_index < 0 || packet_index >= packets.size()) {
      return nullptr;
  }
  return packets[packet_index++];
}

uint64_t hls::Session::get_current_time() {
  if (current_pkt) {
    return active_segment->get_current_time();
  }
  return 0;
}

TSDemux::STREAM_PKT* hls::Session::get_current_pkt() {
  if (!current_pkt) {
    read_next_pkt();
  }
  return current_pkt;
}

void hls::Session::read_next_pkt() {
  if (active_segment) {
    current_pkt = active_segment->get_next_pkt();
    if (!current_pkt && load_segments()) {
      current_pkt = active_segment->get_next_pkt();
    } else if (!current_pkt) {
      if (active_segment) {
        delete active_segment;
        active_segment = 0;
      }
      if (previous_segment) {
        delete previous_segment;
        previous_segment = 0;
      }
    }
  } else {
    current_pkt = nullptr;
  }
}

std::string hls::Session::download_aes_key(std::string aes_uri) {
  std::ifstream file(aes_uri);
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  return std::string(ostrm.str());
}

bool hls::Session::download_segment(ActiveSegment *active_segment) {
  std::ifstream file(active_segment->get_url());
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  active_segment->write_data(ostrm.str().c_str(), ostrm.str().length());
  return true;
}

hls::MediaPlaylist hls::Session::download_playlist(std::string url) {
  FileMediaPlaylist media_playlist;
  media_playlist.open(url.c_str());
  return media_playlist;
}

uint32_t hls::Session::get_best_variant_stream() {
  uint32_t bandwith_of_current_stream = media_playlists.at(active_media_playlist_index).bandwidth;
  uint32_t next_variant_media_playlist_index = active_media_playlist_index;
  for(uint32_t i = 0; i < media_playlists.size(); ++i) {
    MediaPlaylist &media_playlist = media_playlists.at(i);
    if (media_playlist.bandwidth > bandwith_of_current_stream && media_playlist.bandwidth < download_speed) {
       bandwith_of_current_stream = media_playlist.bandwidth;
       next_variant_media_playlist_index = i;
       std::cout << "Variant stream " << i << " bandwidth: " << media_playlist.bandwidth << "\n";
    }
  }
  if (next_variant_media_playlist_index != variant_media_playlist_index) {
    std::cout << "Reloading variant playlist " << variant_media_playlist_index << "\n";
    reload_media_playlist(next_variant_media_playlist_index);
  }
  return next_variant_media_playlist_index;
}

void hls::Session::reload_media_playlist(uint32_t media_playlist_index) {
  MediaPlaylist &media_playlist = media_playlists.at(media_playlist_index);
  std::cout << "Reloading playlist at " << media_playlist_index << " bandwidth: " << media_playlist.bandwidth << "\n";
  if (media_playlist.live) {
     MediaPlaylist new_media_playlist = download_playlist(media_playlist.get_url());
     std::vector<Segment> new_segments = new_media_playlist.get_segments();
     uint32_t last_media_sequence;
     if (media_playlist.get_number_of_segments() > 0) {
       media_playlist.get_segments().back().media_sequence;
     } else {
       // TODO: Will skip the first segment
       last_media_sequence = 0;
     }
     uint32_t added_segments = 0;
     uint32_t last_added_sequence = 0;
     for(std::vector<Segment>::iterator it = new_segments.begin(); it != new_segments.end(); ++it) {
         if (it->media_sequence > last_media_sequence) {
             media_playlist.add_segment(*it);
             ++added_segments;
             last_added_sequence = it->media_sequence;
         }
     }
     std::cout << "Reloaded playlist with " << added_segments << " new segments, last segment id: " << last_added_sequence << "\n";
  }
}

void hls::Session::reload_media_playlist() {
  reload_media_playlist(active_media_playlist_index);
  if (variant_media_playlist_index != -1) {
    reload_media_playlist(variant_media_playlist_index);
  }
}

void hls::Session::check_switch_to_variant_stream() {
  if (variant_media_playlist_index >= 0 && variant_media_playlist_index < media_playlists.size() && variant_media_playlist_index != active_media_playlist_index) {
    MediaPlaylist &variant_playlist = media_playlists.at(variant_media_playlist_index);
    // Check for the next segment in this playlist, if we have it then switch
    // TODO: This check should really be based on media sequence not the absolute number of segments
    // because it won't work for live streams
    if (active_media_segment_index < variant_playlist.get_number_of_segments()) {
      active_media_playlist_index = variant_media_playlist_index;
      std::cout << "Switching to variant playlist " << variant_media_playlist_index << "\n";
    }
  }
}

void hls::Session::create_next_segment_future() {
  variant_media_playlist_index = get_best_variant_stream();
  check_switch_to_variant_stream();
  MediaPlaylist &media_playlist = media_playlists.at(active_media_playlist_index);
  if (active_media_segment_index < 0 || active_media_segment_index >= media_playlist.get_number_of_segments()) {
    // Try to reload the playlist before bailing
    reload_media_playlist();
    if (active_media_segment_index >= media_playlist.get_number_of_segments()) {
      std::cerr << "active_media_segment_index is out of range" << std::endl;
      next_segment_future = std::future<ActiveSegment*>();
      return;
    }
  }
  Segment segment = media_playlist.get_segments()[active_media_segment_index];
  std::cout << "Loading segment " << segment.media_sequence << "\n";
  next_segment_future = std::async(std::launch::async, &hls::Session::load_next_segment, this, segment);
}

hls::ActiveSegment* hls::Session::load_next_segment(hls::Segment segment) {
  std::cout << "Getting segment " << segment.media_sequence << "\n";
  hls::ActiveSegment *next_segment = new hls::ActiveSegment(segment);
  if (!download_segment(next_segment)) {
    std::cerr << "Unable to download active segment"  << std::endl;
  }
  if (segment.encrypted) {
      // TODO: Needs to be protected with a mutex or something
      auto aes_key_it = aes_uri_to_key.find(segment.aes_uri);
      if (aes_key_it == aes_uri_to_key.end()) {
          std::cout << "Getting AES Key from " << segment.aes_uri << "\n";
          std::string aes_key = download_aes_key(segment.aes_uri);
          aes_uri_to_key.insert({segment.aes_uri, aes_key});
          next_segment->create_demuxer(aes_key);
      } else {
          next_segment->create_demuxer(aes_key_it->second);
      }
  } else {
      next_segment->create_demuxer();
  }
  return next_segment;
}

bool hls::Session::load_segments() {
  if (previous_segment) {
    delete previous_segment;
  }
  previous_segment = active_segment;
  uint32_t tries = 0;
  while(!next_segment_future.valid() && tries < 10) {
    std::cout << "Invalid next segment future, attempting to get synchronously\n";
    create_next_segment_future();
    if (!next_segment_future.valid() && media_playlists[active_media_playlist_index].live) {
      // Try to reload playlist
      float target_duration = media_playlists[active_media_playlist_index].get_segment_target_duration();
      uint32_t reload_delay = (uint32_t) target_duration * 0.5 * 1000;
      std::cout << "Unable to load the next segment, " << reload_delay << " waiting to reload\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(reload_delay));
    } else {
      break;
    }
    ++tries;
  }
  active_segment = next_segment_future.get();
  ++active_media_segment_index;
  create_next_segment_future();
  if (!active_segment) {
    return false;
  }
  return true;
}

std::vector<hls::Stream> hls::Session::get_streams() {
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_segment) {
    load_segments();
  }
  return active_segment->streams;
}

hls::Stream hls::Session::get_stream(uint32_t stream_id) {
  std::vector<hls::Stream> streams = get_streams();
  for(std::vector<hls::Stream>::iterator it = streams.begin(); it != streams.end(); ++it) {
    if (it->stream_id == stream_id) {
      return *it;
    }
  }
  return hls::Stream();
}

hls::Session::Session(MasterPlaylist master_playlist) :
    active_media_playlist_index(0),
    active_media_segment_index(0),
    master_playlist(master_playlist),
    previous_segment(0),
    active_segment(0),
    total_time(0),
    start_pts(-1),
    current_pkt(0),
    download_speed(0),
    variant_media_playlist_index(-1),
    media_playlists(master_playlist.get_media_playlist()){
  hls::MediaPlaylist media_playlist = media_playlists[active_media_playlist_index];
  std::vector<Segment> segments = media_playlist.get_segments();
  for(std::vector<hls::Segment>::iterator it = segments.begin(); it != segments.end(); ++it) {
    total_time += it->duration;
  }
  std::cout << "Total time is " << total_time << "\n";
}

hls::Session::~Session() {
  if (active_segment) {
    delete active_segment;
  }
  if (previous_segment) {
    delete previous_segment;
  }
  if (next_segment_future.valid()) {
      delete next_segment_future.get();
  }
}
