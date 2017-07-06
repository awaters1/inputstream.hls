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

#include "hls/HLS.h"

class DemuxContainer {
public:
  DemuxContainer() : demux_packet(0),
  discontinuity(false),
  current_time(0),
  time_in_playlist(0) {
    stream_ids.m_streamCount = 0;
  };
  DemuxPacket *demux_packet;
  uint64_t current_time;
  double time_in_playlist;
  bool discontinuity;
  INPUTSTREAM_IDS stream_ids;
  INPUTSTREAM_INFO streams[INPUTSTREAM_IDS::MAX_STREAM_COUNT];
};
