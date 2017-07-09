#pragma once
/*
 *      Copyright (C) 2013-2016 Team Kodi
 *      http://www.kodi.tv
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
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "tsDemuxer.h"

#include <map>
#include <set>
#include <vector>

#include "kodi_inputstream_types.h"
#include "../demux_container.h"
#include "../hls/SegmentReader.h"

#define AV_BUFFER_SIZE          131072

const int DEMUX_BUFFER_SIZE = 100;

enum DemuxStatus {
  SEGMENT_DONE,
  FILLED_BUFFER,
  ERROR,
  STREAM_SETUP_COMPLETE,
  FLUSH
};

class Demux : public TSDemux::TSDemuxer
{
public:
  Demux();
  ~Demux();

  void Abort();
  DemuxContainer Read(bool remove_packet = true);
  uint32_t get_current_media_sequence();
  void set_segment_reader(std::unique_ptr<SegmentReader> segment_reader);
  DemuxStatus Process(std::vector<DemuxContainer> &demux_packets);
private:
  const unsigned char* ReadAV(uint64_t pos, size_t n);
  void update_timing_data(DemuxContainer &demux_container);
private:
  uint16_t m_channel;
  INPUTSTREAM_IDS m_streamIds;
  INPUTSTREAM_INFO m_streams[INPUTSTREAM_IDS::MAX_STREAM_COUNT];

  bool get_stream_data(TSDemux::STREAM_PKT* pkt);

  void populate_pvr_streams();
  bool update_pvr_stream(uint16_t pid);
  DemuxContainer get_stream_change();
  DemuxPacket* stream_pvr_data(TSDemux::STREAM_PKT* pkt);
  void process_demux_thread();
  bool should_process_demux();

  // AV raw buffer
  size_t m_av_buf_size;         ///< size of av buffer
  uint64_t m_av_pos;            ///< absolute position in av
  unsigned char* m_av_buf;      ///< buffer
  unsigned char* m_av_rbs;      ///< raw data start in buffer
  unsigned char* m_av_rbe;      ///< raw data end in buffer

  // Playback context
  TSDemux::AVContext* m_AVContext;
  uint16_t m_mainStreamPID;     ///< PID of main stream
  int64_t m_readTime;           ///< current relative position based on packets read (DVD_TIME_BASE)
  std::set<uint16_t> m_nosetup;

  std::unique_ptr<SegmentReader> segment_reader;
  bool include_discontinuity;
  SegmentReaderStatus reader_status;
};
