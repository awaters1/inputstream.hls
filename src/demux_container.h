/*
 * demux_container.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

class DemuxContainer {
public:
  DemuxContainer() : demux_packet(0), pcr(0) {};
  DemuxPacket *demux_packet;
  uint64_t pcr;
};
