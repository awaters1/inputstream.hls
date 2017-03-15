/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>

#include "decrypter.h"

#include "session.h"

bool hls::ActiveSegment::write_data(const void *buffer, size_t buffer_size) {
  segment_buffer += std::string((const char *)buffer, buffer_size);
  return true;
}

std::vector<hls::Stream> hls::ActiveSegment::extract_streams() {
  std::vector<hls::Stream> streams;
  TSDemux::STREAM_PKT *pkt;
  while (pkt = demux->get_next_pkt()) {
    if (pkt->streamChange) {
      TSDemux::ElementaryStream *es = demux->get_elementary_stream(pkt->pid);
      Stream stream;
      stream.stream_id = pkt->pid;
      stream.codec_name = es->GetStreamCodecName();
      stream.channels = es->stream_info.channels;
      stream.sample_rate = es->stream_info.sample_rate;
      stream.bit_rate = es->stream_info.bit_rate;
      stream.bits_per_sample = es->stream_info.bits_per_sample;
      streams.push_back(stream);
    }
    delete pkt;
  }
  delete demux;
  demux = new Demux(segment_buffer, 0);
  return streams;
}

void hls::ActiveSegment::create_demuxer(std::string aes_key) {
  if (segment.encrypted) {
    // TODO: If IV is missing the media sequence is the IV
    segment_buffer = decrypt(aes_key, segment.aes_iv, segment_buffer);
  }
  create_demuxer();
}

void hls::ActiveSegment::create_demuxer() {
  demux = new Demux(segment_buffer, 0);
}

hls::ActiveSegment::~ActiveSegment() {
  if (demux) {
    delete demux;
  }
}

TSDemux::STREAM_PKT* hls::ActiveSegment::get_next_pkt() {
  return demux->get_next_pkt();
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
  if (current_pkt && current_pkt->streamChange) {
    current_pkt->streamChange = false;
  } else {
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
        if (next_segment) {
          delete next_segment;
          next_segment = 0;
        }
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
      std::cout << "Reloaded playlist with " << added_segments << " new segments\n";
  }
}

hls::ActiveSegment* hls::Session::load_next_segment() {
  std::cout << "Loading segment " << active_media_segment_index << "\n";
  MediaPlaylist media_playlist = media_playlists.at(active_media_playlist_index);
  if (active_media_segment_index < 0 || active_media_segment_index >= media_playlist.get_segments().size()) {
    std::cerr << "active_media_segment_index is out of range" << std::endl;
    return nullptr;
  }
  Segment segment = media_playlist.get_segments()[active_media_segment_index];
  ActiveSegment *active_segment = new ActiveSegment(segment);
  if (!download_segment(active_segment)) {
    std::cerr << "Unable to download active segment"  << std::endl;
  }
  if (segment.encrypted) {
      auto aes_key_it = aes_uri_to_key.find(segment.aes_uri);
      if (aes_key_it == aes_uri_to_key.end()) {
          std::cout << "Getting AES Key from " << segment.aes_uri << "\n";
          std::string aes_key = download_aes_key(segment.aes_uri);
          aes_uri_to_key.insert({segment.aes_uri, aes_key});
          active_segment->create_demuxer(aes_key);
      } else {
          active_segment->create_demuxer(aes_key_it->second);
      }
  } else {
      active_segment->create_demuxer();
  }
  ++active_media_segment_index;
  reload_media_playlist();
  return active_segment;
}

bool hls::Session::load_segments() {
  if (previous_segment) {
    delete previous_segment;
  }
  previous_segment = active_segment;
  if (!next_segment) {
    next_segment = load_next_segment();
  }
  active_segment = next_segment;
  next_segment = load_next_segment();
  if (!active_segment) {
    return false;
  }
  return true;
}

std::vector<hls::Stream> hls::Session::get_streams() {
  if (!streams.empty()) {
    return streams;
  }
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_segment) {
    load_segments();
  }
  streams = active_segment->extract_streams();
  return streams;
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
    next_segment(0),
    total_time(0),
    start_pts(-1),
    current_pkt(0),
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
  if (next_segment) {
    delete next_segment;
  }
}
