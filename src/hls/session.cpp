/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <sstream>

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

bool hls::Session::download_segment(ActiveSegment *active_segment) {
  std::ifstream file(active_segment->get_url());
  std::ostringstream ostrm;

  ostrm << file.rdbuf();
  active_segment->write_data(ostrm.str().c_str(), ostrm.str().length());
}

hls::ActiveSegment* hls::Session::load_next_segment() {
  std::cout << "Loading segment " << active_media_segment_index << "\n";
  MediaPlaylist media_playlist = master_playlist.get_media_playlist()[active_media_playlist_index];
  if (active_media_segment_index < 0 || active_media_segment_index >= media_playlist.get_segments().size()) {
    std::cerr << "active_media_segment_index is out of range" << std::endl;
    return nullptr;
  }
  Segment segment = media_playlist.get_segments()[active_media_segment_index];
  ActiveSegment *active_segment = new ActiveSegment(segment);
  if (!download_segment(active_segment)) {
    std::cerr << "Unable to download active segment"  << std::endl;
  }
  active_segment->create_demuxer();
  ++active_media_segment_index;
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
    current_pkt(0) {

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
