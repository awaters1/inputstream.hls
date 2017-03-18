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

#include "../tsdemuxer/main.h"

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
    // TODO: If IV is missing the media sequence is the IV
    segment_buffer = decrypt(aes_key, segment.aes_iv, segment_buffer);
  }
  create_demuxer();
}

bool packet_sorter(TSDemux::STREAM_PKT *pkt1, TSDemux::STREAM_PKT *pkt2) {
  return pkt1->dts < pkt2->dts;
}

void hls::ActiveSegment::create_demuxer() {
  demux = new Demux(segment_buffer, 0);
  TSDemux::STREAM_PKT* pkt = 0;
  while (pkt = demux->get_next_pkt()) {
      unsigned char *data = new unsigned char[pkt->size];
      memcpy(data, pkt->data, pkt->size);
      pkt->data = data;
      packets.push_back(pkt);
  }
  // TODO: Sort by DTS
  std::vector<TSDemux::STREAM_PKT*> pkts = get_demux_packets(segment_buffer);
  packets = pkts;
  std::sort(packets.begin(), packets.end(), packet_sorter);


}

hls::ActiveSegment::~ActiveSegment() {
  // TODO: Leaks memory
  /*
  if (demux) {
    delete demux;
  }
  for(std::vector<TSDemux::STREAM_PKT*>::iterator it = packets.begin(); it != packets.end(); ++it) {
      delete *it;
  }
  */
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

hls::Packet* hls::Session::get_current_pkt() {
  if (!current_pkt) {
    read_next_pkt();
  }
  return current_pkt;
}

void hls::Session::read_next_pkt() {
  if (current_pkt && current_pkt->stream_change_flag) {
    current_pkt->stream_change_flag = false;
  } else {
    if (active_segment) {
      TSDemux::STREAM_PKT *pkt = active_segment->get_next_pkt();
      if (!pkt && load_segments()) {
        pkt = active_segment->get_next_pkt();
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
      if (pkt) {
        current_pkt = new Packet(pkt);
      } else {
        current_pkt = nullptr;
      }
    } else {
      current_pkt = nullptr;
    }
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

void hls::Session::reload_media_playlist() {
  MediaPlaylist &media_playlist = media_playlists.at(active_media_playlist_index);
  if (media_playlist.live) {
      MediaPlaylist new_media_playlist = download_playlist(media_playlist.get_url());
      std::vector<Segment> new_segments = new_media_playlist.get_segments();
      uint32_t last_media_sequence = media_playlist.get_segments().back().media_sequence;
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

void hls::Session::create_next_segment_future() {
  std::cout << "Loading segment " << active_media_segment_index << "\n";
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
  next_segment_future =std::async(std::launch::async, &hls::Session::load_next_segment, this, segment);
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
  if (!active_segment->streams.empty()) {
    return active_segment->streams;
  }
  active_segment->extract_streams();

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
    send_previous_packet(false),
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
}
