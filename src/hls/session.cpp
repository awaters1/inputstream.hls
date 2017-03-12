/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <sstream>

#include "session.h"

void hls::ActiveSegment::download_segment() {
  if (segment->get_url().find("http") == 0) {
    // TODO: Download the file
  } else {
    std::ifstream file(segment->get_url());
    std::ostringstream ostrm;

    ostrm << file.rdbuf();
    segment_buffer = std::string(ostrm.str());
  }
}

std::vector<hls::Stream*> hls::ActiveSegment::extract_streams() {
    std::vector<hls::Stream*> streams;
    if (segment_buffer.empty()) {
       download_segment();
    }
    demux = new Demux(segment_buffer, 0);
    TSDemux::STREAM_PKT *pkt;
    while(pkt = demux->get_next_pkt()) {
      if (pkt->streamChange) {

      }
    }
    demux->reset_buffer();
	return streams;
}

hls::ActiveSegment::~ActiveSegment() {
  if (demux) {
    delete demux;
  }
}


std::vector<hls::Stream*> hls::Session::get_streams() {
  if (!streams.empty()) {
      return streams;
  }
  // Load the first segment of the active playlactive_segmentist to obtain the streams
  // from the mpeg2ts
  if (!active_segment) {
      MediaPlaylist *media_playlist = master_playlist.get_media_playlist()[active_media_playlist_index];
      Segment *segment = media_playlist->get_segments()[active_media_segment_index];
      active_segment = new ActiveSegment(segment);
  }
  streams = active_segment->extract_streams();
  return streams;
}

hls::Session::Session(MasterPlaylist master_playlist)
: active_media_playlist_index(0), active_media_segment_index(0), master_playlist(master_playlist), active_segment(0) {

}

hls::Session::~Session() {
  for(std::vector<Stream*>::iterator it = streams.begin(); it != streams.end(); ++it) {
      delete *it;
  }
  if (active_segment) {
      delete active_segment;
  }
}
