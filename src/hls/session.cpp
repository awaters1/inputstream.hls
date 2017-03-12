/*
 * session.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "session.h"

std::vector<hls::Stream*> hls::Session::get_streams() {
  if (!streams.empty()) {
      return streams;
  }
  // Load the first segment of the active playlist to obtain the streams
  // from the mpeg2ts
  // TODO: Read the whole segment and send it to the demuxer to get the
  // elementary streams
  if (!active_segment) {
      active_segment = new ActiveSegment();
  }
//  if (demux) {
//      delete demux;
//  }
//  bool downloaded = dashStream->download_segment();
//  demux = new Demux(dashStream->segment_buffer_, 0);

  return streams;
}

hls::Session::Session()
: active_media_playlist(0) {

}

hls::Session::~Session() {
  for(std::vector<Stream*>::iterator it = streams.begin(); it != streams.end(); ++it) {
      delete *it;
  }
  if (active_segment) {
      delete active_segment;
  }
}
