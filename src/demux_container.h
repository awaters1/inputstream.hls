/*
 * demux_container.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

class DemuxContainer {
public:
  DemuxPacket *demux_packet;
  uint64_t pcr;
};
